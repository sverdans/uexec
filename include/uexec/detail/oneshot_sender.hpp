#pragma once

#include <type_traits>

#include <execution.hpp>

#include <uexec/uring.hpp>
#include <uexec/meta.hpp>
#include <uexec/detail/cancellation.hpp>

namespace uexec {
namespace detail {

template <auto Prepare, typename... Args>
class oneshot_sender {
private:
	using args_t = std::tuple<Args...>;

public:
	using sender_concept = ex::sender_tag;

	template <class Self, class Env>
	static consteval auto get_completion_signatures() noexcept {
		return ex::completion_signatures<
			ex::set_value_t(io_uring_cqe*),
			ex::set_error_t(std::error_code),
			ex::set_stopped_t()
		>{};
	}

	template <typename Receiver>
	class operation_state final : public operation_base {
	public:
		operation_state(uring& ctx, args_t&& args, Receiver&& rcvr)
			: _ctx(ctx)
			, _args(std::move(args))
			, _rcvr(std::move(rcvr)) {}

		void start() noexcept {
			auto env = ex::get_env(_rcvr);
			ex::stoppable_token auto token = ex::get_stop_token(env);
			if (token.stop_requested()) [[unlikely]] {
				ex::set_stopped(std::move(_rcvr));
				return;
			}

			io_uring_sqe* sqe = io_uring_get_sqe(_ctx.handle());
			if (sqe) [[likely]] {
				prepare_sqe(sqe);
				io_uring_sqe_set_data(sqe, static_cast<operation_base*>(this));
				_cancellation.enable(token, _ctx, this);
				return;
			}

			ex::set_error(std::move(_rcvr), system_error(ENOMEM));
		}

		void complete(io_uring_cqe* cqe) noexcept {
			_cancellation.disable();
			ex::set_value(std::move(_rcvr), cqe);
		}

	private:
		void prepare_sqe(io_uring_sqe* sqe) noexcept {
			std::apply([sqe](auto&... args) noexcept { Prepare(sqe, args...); }, _args);
		}

	private:
		using token_t = stop_token_t<Receiver>;
		uring& _ctx;
		args_t _args;
		Receiver _rcvr;
		cancellation<token_t> _cancellation;
	};

public:
	oneshot_sender(uring& ctx, Args... args)
		: _ctx(ctx)
		, _args(args...) {}

	template <ex::receiver Receiver>
	auto connect(Receiver&& rcvr) noexcept {
		using receiver_type = std::remove_cvref_t<Receiver>;
		return operation_state<receiver_type>(_ctx, std::move(_args), std::move(rcvr));
	}

private:
	uring& _ctx;
	args_t _args;
};

template <auto Prepare, typename... Args>
auto make_oneshot_sender(uring& ctx, Args... args) {
	return oneshot_sender<Prepare, std::decay_t<Args>...>(ctx, args...);
}

} // namespace detail
} // namespace uexec
