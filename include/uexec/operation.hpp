#pragma once

#include <netinet/in.h>

#include <execution.hpp>

#include <uexec/sender.hpp>

namespace uexec {

struct connect_op {
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

constexpr inline auto connect = make_uring_sender_t<connect_op>();

} // namespace uexec
