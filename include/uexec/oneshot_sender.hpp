#pragma once

#include <type_traits>

#include <execution.hpp>

#include <uexec/uring.hpp>
#include <uexec/detail/operation_concept.hpp>

namespace uexec {

// TODO:
// - noexcept
// - вывод типов и forwarding в конструкторах
// - концепты

template <typename T>
struct operation_value {
	using type = decltype(std::declval<T>().complete(nullptr));
};

template <typename T>
using operation_value_t = typename operation_value<T>::type;

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

struct cancel_callback {
public:
	cancel_callback(uring& ctx, void* target) noexcept
		: _ctx(ctx)
		, _target(target) {}

	void operator()() noexcept {
		io_uring_sqe* sqe = io_uring_get_sqe(_ctx.handle());
		if (sqe) {
			// Возможные сценарии отмены операции:
			// 1. сначала формируется cqe cancel_operation, cqe целевой операции вернётся с кодом ECANCEL
			// 2. сначала формируется cqe целевой операции, cqe cancel_operation будет содержать код ошибки
			// заключения:
			// - невозможно продолжить граф исполнения для сценария 2, так как cancel_operation будет уничтожен;
			// - если отмена прошла штатно, то анализировать cqe cancel_operation нет необходимости,
			//   так как cqe целевой операции будет содержать errc ECANCEL.
			io_uring_prep_cancel(sqe, _target, 0);
			io_uring_sqe_set_data(sqe, nullptr);
		}
	}

private:
	uring& _ctx;
	void* _target;
};

template <uring_operation Operation>
class oneshot_sender {
public:
	using sender_concept = ex::sender_tag;
	using value_t = operation_value_t<Operation>;

	template <class Self, class Env>
	static consteval auto get_completion_signatures() noexcept {
		using signature_t = typename operation_completion_signatures<value_t>::type;
		return signature_t{};
	}

	template <ex::receiver Receiver>
	class operation_state final : public operation_base {
	private:
		using env_t           = ex::env_of_t<Receiver>;
		using stop_token_t    = ex::stop_token_of_t<env_t>;
		using stop_callback_t = typename stop_token_t::template callback_type<cancel_callback>;

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
				_stop.emplace(token, cancel_callback(_ctx, this));
				return;
			}

			ex::set_error(std::move(_rcvr), system_error(ENOMEM));
		}

		void complete(io_uring_cqe* cqe) noexcept {
			_stop.reset();
			int res = cqe->res;
			if (res >= 0) [[likely]] {
				set_value(cqe);
			} else if (res == -ECANCELED) {
				ex::set_stopped(std::move(_rcvr)); // TODO: проверить что отмена была запрошена
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
		uring& _ctx;
		Operation _operation;
		Receiver _rcvr;
		std::optional<stop_callback_t> _stop;
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
