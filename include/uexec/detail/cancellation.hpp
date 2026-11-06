#pragma once

#include <optional>

#include <execution.hpp>

#include <uexec/uring.hpp>

namespace uexec {
namespace detail {

template <ex::stoppable_token StopToken>
class cancellation {
	struct cancel_callback {
		void operator()() const noexcept { owner->request_stop(); }

		cancellation* owner;
	};

	using stop_callback_t = typename StopToken::template callback_type<cancel_callback>;

public:
	class stop_handle {
	public:
		explicit stop_handle(cancellation& owner) noexcept : _owner(&owner) {}

		bool request_stop() const noexcept { return _owner->request_stop(); }

		bool stop_requested() const noexcept { return _owner->requested(); }

	private:
		cancellation* _owner;
	};

	stop_handle get_handle() noexcept { return stop_handle(*this); }

	bool request_stop() noexcept {
		if (_in_progress) {
			return true;
		}

		_requested = true;
		io_uring_sqe* sqe = io_uring_get_sqe(_ctx->handle()); // TODO: переповтор
		if (not sqe) {
			return false;
		}

		// Возможные сценарии отмены операции:
		// 1. сначала формируется cqe cancel_operation, cqe целевой операции вернётся с кодом ECANCEL
		// 2. сначала формируется cqe целевой операции, cqe cancel_operation будет содержать код ошибки
		// заключения:
		// - невозможно продолжить граф исполнения для сценария 2, так как cancel_operation будет уничтожен;
		// - если отмена прошла штатно, то анализировать cqe cancel_operation нет необходимости,
		//   так как cqe целевой операции будет содержать errc ECANCEL.
		_in_progress = true;
		io_uring_prep_cancel(sqe, _target, 0);
		io_uring_sqe_set_data(sqe, nullptr);
		return true;
	}

	void enable(StopToken token, uring& ctx, operation_base* target) noexcept {
		_ctx = &ctx;
		_target = target;
		_cb.emplace(token, cancel_callback{this});
	}

	void disable() noexcept { _cb.reset(); }

	bool requested() const noexcept { return _requested; }

private:
	uring* _ctx       = nullptr;
	void* _target     = nullptr;
	bool _requested   = false;
	bool _in_progress = false;
	std::optional<stop_callback_t> _cb;
};

} // namespace detail
} // namespace uexec
