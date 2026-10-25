#pragma once

#include <optional>

#include <execution.hpp>

#include <uexec/uring.hpp>

namespace uexec {
namespace detail {

struct cancel_callback {
	void operator()() noexcept {
		_requested = true;
		io_uring_sqe* sqe = io_uring_get_sqe(_ctx.handle()); // TODO: переповтор
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

	uring& _ctx;
	void* _target;
	bool& _requested;
};

template <ex::stoppable_token StopToken>
class cancellation {
	using stop_callback_t = typename StopToken::template callback_type<cancel_callback>;
public:
	void enable(StopToken token, uring& ctx, operation_base* target) noexcept {
		_cb.emplace(token, cancel_callback{ctx, static_cast<void*>(target), _requested});
	}

	void disable() noexcept { _cb.reset(); }

	bool requested() const noexcept { return _requested; }

private:
	bool _requested = false;
	std::optional<stop_callback_t> _cb;
};

} // namespace detail
} // namespace uexec
