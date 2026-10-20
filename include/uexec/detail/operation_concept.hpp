#pragma once

#include <concepts>
#include <utility>

#include <execution.hpp>

struct io_uring_sqe;
struct io_uring_cqe;

namespace uexec {

template <typename Op>
concept has_nothrow_prepare =
	requires(Op& op, io_uring_sqe* sqe) {
		{ op.prepare(sqe) } noexcept;
	};

template <typename Op>
concept has_nothrow_complete =
	requires(Op& op, io_uring_cqe* cqe) {
		{ op.complete(cqe) } noexcept;
	};

template <typename Op>
concept uring_operation =
	has_nothrow_prepare<Op> &&
	has_nothrow_complete<Op>;

} // namespace uexec
