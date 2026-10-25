#pragma once

#include <type_traits>

#include <execution.hpp>

#include <uexec/uring.hpp>
#include <uexec/meta.hpp>
#include <uexec/detail/operation_concept.hpp>
#include <uexec/detail/cancellation.hpp>

// TODO:
// - noexcept
// - вывод типов и forwarding в конструкторах
// - концепты

namespace uexec {
namespace detail {

template <typename T>
struct operation_completion_signatures {
	using type = ex::completion_signatures<
		ex::set_value_t(T),
		ex::set_error_t(std::error_code),
		ex::set_stopped_t()
	>;
};

template <>
struct operation_completion_signatures<void> {
	using type = ex::completion_signatures<
		ex::set_value_t(),
		ex::set_error_t(std::error_code),
		ex::set_stopped_t()
	>;
};

template <typename T>
using operation_completion_signatures_t = typename operation_completion_signatures<T>::type;

} // namespace detail

template <uring_operation Operation>
class oneshot_sender {
public:
	using sender_concept = ex::sender_tag;
	using value_t = operation_value_t<Operation>;

	template <class Self, class Env>
	static consteval auto get_completion_signatures() noexcept {
		return detail::operation_completion_signatures_t<value_t>{};
	}

	template <typename Receiver>
	class operation_state final : public operation_base {
	public:
		operation_state(uring& ctx, Operation&& operation, Receiver&& rcvr)
			: _ctx(ctx)
			, _operation(std::forward<Operation>(operation))
			, _rcvr(std::forward<Receiver>(rcvr)) {}

		void start() noexcept {
			auto env = ex::get_env(_rcvr);
			ex::stoppable_token auto token = ex::get_stop_token(env);
			if (token.stop_requested()) [[unlikely]] {
				ex::set_stopped(std::move(_rcvr));
				return;
			}

			io_uring_sqe* sqe = io_uring_get_sqe(_ctx.handle());
			if (sqe) [[likely]] {
				_operation.prepare(sqe);
				io_uring_sqe_set_data(sqe, static_cast<operation_base*>(this));
				_cancellation.enable(token, _ctx, this);
				return;
			}

			ex::set_error(std::move(_rcvr), system_error(ENOMEM));
		}

		void complete(io_uring_cqe* cqe) noexcept {
			_cancellation.disable();
			int res = cqe->res;
			if (res >= 0) [[likely]] {
				set_value(cqe);
			} else if (_cancellation.requested() && res == -ECANCELED) {
				ex::set_stopped(std::move(_rcvr));
			} else {
				ex::set_error(std::move(_rcvr), system_error(-res));
			}
		}

	private:
		void set_value(io_uring_cqe* cqe) noexcept {
			if constexpr (std::is_same_v<value_t, void>) {
				_operation.complete(cqe);
				ex::set_value(std::move(_rcvr));
			} else {
				ex::set_value(std::move(_rcvr), _operation.complete(cqe));
			}
		}

	private:
		using token_t = stop_token_t<Receiver>;
		uring& _ctx;
		Operation _operation;
		Receiver _rcvr;
		detail::cancellation<token_t> _cancellation;
	};

public:
	oneshot_sender(uring& ctx, Operation&& operation)
		: _ctx(ctx)
		, _operation(std::forward<Operation>(operation)) {}

	template <ex::receiver Receiver>
	auto connect(Receiver&& rcvr) noexcept {
		using receiver_type = std::remove_cvref_t<Receiver>;
		return operation_state<receiver_type>(_ctx, std::move(_operation), std::move(rcvr));
	}

private:
	uring& _ctx;
	Operation _operation;
};

template <uring_operation Operation>
struct make_oneshot_sender_t {
	using sender_type = oneshot_sender<Operation>;

	template <typename... Args>
	sender_type operator()(uring& ctx, Args&&... args) const noexcept {
		static_assert(
			std::constructible_from<Operation, Args...>,
			"Operation cannot be constructed from the given arguments"
		);
		return sender_type{ctx, Operation{std::forward<Args>(args)...}};
	}

	template <typename... Args>
	auto operator()(Args&&... args) const noexcept {
		return ex::read_env(ex::get_start_scheduler) |
			ex::let_value(
				[args = std::tuple<std::decay_t<Args>...>(std::forward<Args>(args)...)]
				(auto sched) noexcept {
					return std::apply(
						[&](auto&&... unpacked) noexcept {
							return sender_type(
								sched.context(),
								Operation(std::forward<decltype(unpacked)>(unpacked)...)
							);
						},
						std::move(args)
					);
				}
			);
	}
};

} // namespace uexec
