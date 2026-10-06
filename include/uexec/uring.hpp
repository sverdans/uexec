#pragma once

#include <utility>

#include <sys/signalfd.h>
#include <sys/poll.h>

#include <liburing.h>

#include <uexec/types.hpp>
#include <uexec/system_error.hpp>

#include <execution.hpp>

namespace uexec {

class operation_base {
public:
	virtual void complete(io_uring_cqe* cqe) noexcept = 0;
};

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

template <typename Operation>
class submission_sender {
public:
	using sender_concept = ex::sender_tag;

	template <class Self, class Env>
	static consteval auto get_completion_signatures() noexcept {
		using value_t = typename Operation::value_type;
		using signature_t = typename operation_completion_signatures<value_t>::type;
		return signature_t{};
	}

	template <typename Receiver>
	class operation_state final : public operation_base {
	private:
		struct cancel_operation {
		public:
			cancel_operation(io_uring* uring, operation_state* target) noexcept
				: _uring(uring)
				, _target(target) {}

			void operator()() noexcept {
				io_uring_sqe* sqe = io_uring_get_sqe(_uring);
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
			io_uring* _uring;
			operation_state* _target;
		};

		using env_t           = ex::env_of_t<Receiver>;
		using stop_token_t    = ex::stop_token_of_t<env_t>;
		using stop_callback_t = typename stop_token_t::template callback_type<cancel_operation>;

	public:
		operation_state(io_uring* uring, Operation operation, Receiver rcvr)
			: _uring(uring)
			, _operation(operation)
			, _rcvr(rcvr) {}

		void start() noexcept {
			auto env = ex::get_env(_rcvr);
			ex::stoppable_token auto token = ex::get_stop_token(env);
			if (token.stop_requested()) {
				ex::set_stopped(std::move(_rcvr));
				return;
			}

			io_uring_sqe* sqe = io_uring_get_sqe(_uring);
			if (sqe) {
				_operation.prepare(sqe);
				io_uring_sqe_set_data(sqe, static_cast<operation_base*>(this));
				_stop.emplace(token, cancel_operation(_uring, this));
				return;
			}

			ex::set_error(std::move(_rcvr), system_error(ENOMEM));
		}

		void complete(io_uring_cqe* cqe) noexcept {
			_stop.reset();
			_operation.complete(std::move(_rcvr), cqe);
		}

	private:
	// TODO:
	// - noexcept _func
	// - вывод типов и forwarding в конструкторах
		io_uring* _uring;
		Operation _operation;
		Receiver _rcvr;
		std::optional<stop_callback_t> _stop;
	};

public:
	submission_sender(io_uring* uring, Operation&& operation)
		: _uring(uring)
		, _operation(std::forward<Operation>(operation)) {}

	template <ex::receiver Receiver>
	auto connect(Receiver rcvr) noexcept {
		return operation_state<Receiver>(_uring, std::move(_operation), std::move(rcvr));
	}

private:
	io_uring* _uring;
	Operation _operation;
};

struct connect_operation {
	using value_type = void;

	void prepare(io_uring_sqe* sqe) noexcept {
		io_uring_prep_connect(sqe, fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
	}

	template <ex::receiver Receiver>
	void complete(Receiver&& rcvr, io_uring_cqe* cqe) noexcept {
		if (cqe->res >= 0) {
			ex::set_value(std::move(rcvr));
		} else {
			ex::set_error(std::move(rcvr), system_error(-cqe->res));
		}
	}

	int fd;
	sockaddr_in addr;
};

struct poll_add_operation {
	using value_type = void;

	void prepare(io_uring_sqe* sqe) noexcept {
		io_uring_prep_poll_add(sqe, fd, mask);
	}

	template <ex::receiver Receiver>
	void complete(Receiver&& rcvr, io_uring_cqe* cqe) noexcept {
		if (cqe->res >= 0) {
			ex::set_value(std::move(rcvr));
		} else {
			ex::set_error(std::move(rcvr), system_error(-cqe->res));
		}
	}

	int fd;
	unsigned int mask;
};

class uring final {
public:
	class scheduler {
	public:
		template <ex::receiver Receiver>
		class schedule_operation {
		public:
			schedule_operation(uring& ctx, Receiver rcvr)
				: _ctx(ctx)
				, _rcvr(std::move(rcvr)) {}

			void start() noexcept {
				stdexec::set_value(std::move(_rcvr));
			}

		private:
			uring& _ctx;
			Receiver _rcvr;
		};

		class schedule_sender {
		public:
			using sender_concept = stdexec::sender_tag;

			template <class Self, class Env>
			static consteval auto get_completion_signatures() noexcept {
				return stdexec::completion_signatures<stdexec::set_value_t()>{};
			}

			explicit schedule_sender(uring& ctx) noexcept
				: _ctx(ctx) {}

			template <class Receiver>
			auto connect(Receiver rcvr) const {
				return schedule_operation<Receiver>(_ctx, std::move(rcvr));
			}

		private:
			uring& _ctx;
		};

	public:
		scheduler(uring& ctx) noexcept : _ctx(ctx) {}

		auto schedule() const noexcept { return schedule_sender(_ctx); }

		bool operator==(const scheduler& other) const noexcept {
			return &_ctx == &other._ctx;
		}

	private:
		uring& _ctx;
	};

public:
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
	scheduler get_scheduler() noexcept {
		return scheduler(*this);
	}

	class join_receiver {
	public:
		using receiver_concept = stdexec::receiver_tag;

		join_receiver(scheduler sched, bool& done) noexcept
			: _sched(sched)
			, _done(&done) {}

		void set_value() noexcept { *_done = true; }

		template <class E>
		void set_error(E&&) noexcept { *_done = true; }

		void set_stopped() && noexcept { *_done = true; }

		auto get_env() const noexcept {
			return stdexec::env{
				stdexec::prop{stdexec::get_start_scheduler, _sched}
			};
		}

	private:
		scheduler _sched;
		bool* _done;
	};

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
		auto op = ex::connect(_scope.join(), join_receiver(get_scheduler(), done));
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
	template <ex::sender Sender>
	void spawn(Sender&& sndr) {
		stdexec::spawn(std::forward<Sender>(sndr), _scope.get_token());
	}

public:
	auto connect(int fd, sockaddr_in addr) {
		return submission_sender(&_uring, connect_operation{fd, addr});
	}

	auto poll_add(int fd, unsigned int mask) {
		return submission_sender(&_uring, poll_add_operation{fd, mask});
	}

public:
	auto receive_signal(int signal) {
		sigset_t mask{};
		sigemptyset(&mask);
		sigaddset(&mask, signal);
		if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
			perror("sigprocmask");
			//return 1;
		}

		int sfd = signalfd(-1, &mask, SFD_NONBLOCK);
		if (sfd < 0) {
			perror("signalfd");
			//return 1;
		}

		return poll_add(sfd, POLLIN);
	}

private:
	io_uring _uring{};
	size_t _entries;
	bool _stop_requested = false;
	ex::counting_scope _scope;
};

} // namespace uexec
