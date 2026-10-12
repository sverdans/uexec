#pragma once

#include <concepts>
#include <utility>

#include <execution.hpp>

struct io_uring_sqe;
struct io_uring_cqe;

namespace uexec {

template <typename Value>
struct dummy_receiver {
	using receiver_concept = ex::receiver_tag;
	void set_value(dummy_receiver&&, Value&&) noexcept {}
	void set_error(dummy_receiver&&, std::error_code) noexcept {}
	void set_stopped(dummy_receiver&&) noexcept {}
};

template <>
struct dummy_receiver<void> {
	using receiver_concept = ex::receiver_tag;
	void set_value(dummy_receiver&&) noexcept {}
	void set_error(dummy_receiver&&, std::error_code) noexcept {}
	void set_stopped(dummy_receiver&&) noexcept {}
};

template <typename Op>
concept has_value_type =
	requires { typename Op::value_type; };

template <typename Op>
concept has_nothrow_prepare =
	requires(Op& op, io_uring_sqe* sqe) {
		{ op.prepare(sqe) } noexcept;
	};

template <typename Op>
concept has_nothrow_complete =
	has_value_type<Op> &&
	requires(Op& op, io_uring_cqe* cqe, dummy_receiver<typename Op::value_type> rcvr) {
		{ op.complete(std::move(rcvr), cqe) } noexcept;
	};

template <typename Op>
concept uring_operation =
	has_value_type<Op> &&
	has_nothrow_prepare<Op> &&
	has_nothrow_complete<Op>;

} // namespace uexec
