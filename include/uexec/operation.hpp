#pragma once

#include <netinet/in.h>

#include <execution.hpp>

#include <uexec/oneshot_sender.hpp>

namespace uexec {

struct connect_op {
	void prepare(io_uring_sqe* sqe) noexcept {
		io_uring_prep_connect(sqe, fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
	}

	void complete(io_uring_cqe*) noexcept {}

	int fd;
	sockaddr_in addr;
};

struct send_op {
	void prepare(io_uring_sqe* sqe) noexcept {
		io_uring_prep_send(sqe, fd, buf, len, flags);
	}

	size_t complete(io_uring_cqe* cqe) noexcept {
		return static_cast<size_t>(cqe->res);
	}

	int fd;
	const void* buf;
	size_t len;
	int flags = 0;
};

struct sendmsg_op {
	void prepare(io_uring_sqe* sqe) noexcept {
		io_uring_prep_sendmsg(sqe, fd, &msg, flags);
	}

	size_t complete(io_uring_cqe* cqe) noexcept {
		return static_cast<size_t>(cqe->res);
	}

	int fd;
	msghdr msg;
	unsigned int flags = 0;
};

struct recv_op {
	void prepare(io_uring_sqe* sqe) noexcept {
		io_uring_prep_recv(sqe, fd, buf, len, flags);
	}

	size_t complete(io_uring_cqe* cqe) noexcept {
		return static_cast<size_t>(cqe->res);
	}

	int fd;
	void* buf;
	size_t len;
	int flags = 0;
};


constexpr inline auto connect = make_oneshot_sender_t<connect_op>();
constexpr inline auto send    = make_oneshot_sender_t<send_op>();
constexpr inline auto sendmsg = make_oneshot_sender_t<sendmsg_op>();
constexpr inline auto recv    = make_oneshot_sender_t<recv_op>();

} // namespace uexec
