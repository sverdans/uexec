#pragma once

#include <utility>

#include <sys/signalfd.h>
#include <sys/poll.h>

#include <liburing.h>

#include <uexec/types.hpp>
#include <uexec/system_error.hpp>
#include <uexec/detail/flag_receiver.hpp>
#include <uexec/exception_policy.hpp>
#include <uexec/scheduler.hpp>

#include <execution.hpp>

namespace uexec {

class operation_base {
public:
	virtual void complete(io_uring_cqe* cqe) noexcept = 0;
};

class uring final {
public:
	using scheduler = eager_scheduler<uring>;

	uring(size_t entries)
		: _entries(entries) {
		int res = io_uring_queue_init(static_cast<unsigned>(entries), &_uring, 0);
		throw_system_error_if(res < 0, -res);
	}

	uring(const uring&) = delete;

	uring& operator=(const uring&) = delete;

	uring(uring&&) = delete;

	uring& operator=(uring&&) = delete;

	~uring() {
		io_uring_queue_exit(&_uring);
	}

public:
	scheduler get_scheduler() noexcept { return scheduler(*this); }

	io_uring* handle() noexcept { return &_uring; }

	void request_stop() {
		if (not _stop_requested) {
			_stop_requested = true;
			_scope.request_stop();
			_scope.close();
		}
	}

	void run_until_stopped() {
		run_until([this] { return _stop_requested; });

		bool done = false;
		auto op = ex::connect(_scope.join(), detail::flag_receiver(get_scheduler(), done));
		ex::start(op);

		run_until([&done] { return done; });
	}

	template <typename Pred>
	void run_until(Pred&& done) {
		while (not std::invoke(done)) {
			size_t events = run_once();
			if (events == 0) {
				::sleep(0);
			}
		}
	}

	size_t run_once() {
		io_uring_submit(&_uring);
		io_uring_cqe* cqe;
		unsigned head;
		unsigned count = 0;

		io_uring_for_each_cqe(&_uring, head, cqe) {
			++count;
			auto* operation = static_cast<operation_base*>(io_uring_cqe_get_data(cqe));
			if (operation) [[likely]] {
				operation->complete(cqe);
			}
		}

		io_uring_cq_advance(&_uring, count);
		return count;
	}

public:
	template <exception_policy POLICY = exception_policy::standard, ex::sender Sender>
	void spawn(Sender&& sndr) {
		if constexpr (POLICY == exception_policy::abort) {
			ex::spawn(
				with_abort_on_exception(
					ex::starts_on(get_scheduler(), std::move(sndr))
				),
				_scope.get_token()
			);
		} else {
			ex::spawn(
				ex::starts_on(get_scheduler(), std::move(sndr)),
				_scope.get_token()
			);
		}
	}

	template <exception_policy Policy = exception_policy::standard, class Factory>
	requires std::invocable<Factory> && ex::sender<std::invoke_result_t<Factory>>
	void spawn(Factory&& factory) {
		spawn<Policy>(std::invoke(std::forward<Factory>(factory)));
	}

private:
	io_uring _uring{};
	size_t _entries;
	bool _stop_requested = false;
	ex::counting_scope _scope;
};

} // namespace uexec
