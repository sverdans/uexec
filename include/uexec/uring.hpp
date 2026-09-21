#pragma once

#include <utility>

#include <liburing.h>

#include <uexec/types.hpp>
#include <uexec/system_error.hpp>

#include <execution.hpp>

namespace uexec {

class uring final {
public:
	class operation_base {
	public:
		virtual void complete(io_uring_cqe* cqe) noexcept = 0;
	};

public:
	uring(size_t entries)
		: _entries(entries) {
		int res = io_uring_queue_init(2048, &_uring, 0);
		throw_system_error_if(res < 0, -res);
	}

	uring(const uring&) = delete;

	uring& operator=(const uring&) = delete;

	uring(uring&&) = delete;

	uring& operator=(uring&&) = delete;

	~uring() { io_uring_queue_exit(&_uring); }

public:
	io_uring_sqe* get_sqe() noexcept {
		return io_uring_get_sqe(&_uring);
	}

	size_t run() {
		io_uring_submit_and_wait(&_uring, 1);
		return for_each_cqe(
			[](io_uring_cqe* cqe) {
				auto* operation = static_cast<operation_base*>(io_uring_cqe_get_data(cqe));
				if (operation) [[likely]] {
					operation->complete(cqe);
				}
			}
		);
	}

private:
	template <typename Func>
	size_t for_each_cqe(Func&& func) {
		io_uring_cqe* cqe;
		unsigned head;
		unsigned count = 0;

		io_uring_for_each_cqe(&_uring, head, cqe) {
			++count;
			func(cqe);
		}

		io_uring_cq_advance(&_uring, count);
		return count;
	}

private:
	io_uring _uring{};
	size_t _entries;
};

template <typename Func>
class submission_sender {
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
	class operation_state : public uring::operation_base {
	private:
		struct cancel_operation {
		public:
			cancel_operation(operation_state* target) noexcept
				: _target(target) {}

			void operator()() noexcept {
				auto env = ex::get_env(_target->_rcvr);
				scheduler sched = ex::get_scheduler(env);
				io_uring_sqe* sqe = sched.get_context().get_sqe();
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
			operation_state* _target;
		};

		using env_t           = ex::env_of_t<Receiver>;
		using stop_token_t    = ex::stop_token_of_t<env_t>;
		using stop_callback_t = typename stop_token_t::template callback_type<cancel_operation>;

	public:
		operation_state(Receiver rcvr, Func func)
			: _rcvr(rcvr)
			, _func(func) {}

		void start() noexcept {
			auto env = ex::get_env(_rcvr);
			ex::stoppable_token auto token = ex::get_stop_token(env);
			if (token.stop_requested()) {
				ex::set_stopped(std::move(_rcvr));
				return;
			}

			ex::scheduler auto sched = ex::get_scheduler(env);
			io_uring_sqe* sqe = sched.get_context().get_sqe();
			if (sqe) {
				_func(sqe);
				io_uring_sqe_set_data(sqe, this);
				_stop.emplace(token, cancel_operation(this));
				return;
			}

			ex::set_error(std::move(_rcvr), system_error(ENOMEM));
		}

	private:
	// TODO:
	// - noexcept _func
	// - вывод типов и forwarding в конструкторах
		Receiver _rcvr;
		Func _func;
		std::optional<stop_callback_t> _stop;
	};

public:
	submission_sender(Func&& func)
		: _func(std::forward<Func>(func)) {}

	template <ex::receiver Receiver>
	auto connect(Receiver rcvr) noexcept {
		return operation_state<Receiver>(std::move(rcvr), std::move(_func));
	}

private:
	Func _func;
};

auto connect(int fd, sockaddr_in addr) {
	return submission_sender(
		[fd, addr](io_uring_sqe* sqe) {
			io_uring_prep_connect(sqe, fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
		}
	);
}

} // namespace uexec
