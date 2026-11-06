#pragma once

#include <expected>

#include <netinet/in.h>

#include <execution.hpp>

#include <uexec/system_error.hpp>
#include <uexec/detail/oneshot_sender.hpp>
#include <uexec/detail/multishot_sender.hpp>
#include <uexec/detail/complete_with_value.hpp>

namespace uexec {
namespace detail {

template <typename Factory>
struct io_factory : public Factory {
	using Factory::operator();

	template <typename First>
	static constexpr bool is_not_uring = not std::same_as<std::remove_cvref_t<First>, uring>;

	template <typename First, typename... Rest>
	requires (is_not_uring<First>)
	auto operator()(First&& first, Rest&&... rest) const noexcept {
		using tuple_t = std::tuple<
			std::decay_t<First>,
			std::decay_t<Rest>...
		>;

		return ex::read_env(ex::get_start_scheduler) |
			ex::let_value(
				[args = tuple_t(std::forward<First>(first), std::forward<Rest>(rest)...)]
				(auto sched) noexcept {
					return std::apply(
						[&](auto&&... unpacked) noexcept {
							constexpr Factory derived{};
							return derived(sched.context(), std::forward<decltype(unpacked)>(unpacked)...);
						},
						std::move(args)
					);
				}
			);
	}
};

struct listen_t {
	auto operator()(uring& ctx, int fd, int backlog) const noexcept {
		return make_oneshot_sender<io_uring_prep_listen>(ctx, fd, backlog) |
			complete_with_value<void>(
				[](io_uring_cqe* cqe, auto&& receiver) noexcept {
					if (cqe->res >= 0) {
						ex::set_value(std::move(receiver));
					} else {
						ex::set_error(std::move(receiver), system_error(-cqe->res));
					}
				}
			);
	}
};

struct bind_t {
	auto operator()(uring& ctx, int fd, const sockaddr* addr, socklen_t addrlen) const noexcept {
		return make_oneshot_sender<io_uring_prep_bind>(ctx, fd, addr, addrlen) |
			complete_with_value<void>(
				[](io_uring_cqe* cqe, auto&& receiver) noexcept {
					if (cqe->res >= 0) {
						ex::set_value(std::move(receiver));
					} else {
						ex::set_error(std::move(receiver), system_error(-cqe->res));
					}
				}
			);
	}
};

struct accept_t {
	auto operator()(uring& ctx, int fd, sockaddr* addr, socklen_t* addrlen, int flags = 0) const noexcept {
		return make_oneshot_sender<io_uring_prep_accept>(ctx, fd, addr, addrlen, flags) |
			complete_with_value<int>(
				[](io_uring_cqe* cqe, auto&& receiver) noexcept {
					if (cqe->res >= 0) {
						ex::set_value(std::move(receiver), cqe->res);
					} else {
						ex::set_error(std::move(receiver), system_error(-cqe->res));
					}
				}
			);
	}
};

struct connect_t {
	auto operator()(uring& ctx, int fd, const sockaddr* addr, socklen_t addrlen) const noexcept {
		return make_oneshot_sender<io_uring_prep_connect>(ctx, fd, addr, addrlen) |
			complete_with_value<void>(
				[](io_uring_cqe* cqe, auto&& receiver) noexcept {
					if (cqe->res >= 0) {
						ex::set_value(std::move(receiver));
					} else {
						ex::set_error(std::move(receiver), system_error(-cqe->res));
					}
				}
			);
	}
};

struct send_t {
	auto operator()(uring& ctx, int fd, const void* buf, size_t len, int flags = 0) const noexcept {
		return make_oneshot_sender<io_uring_prep_send>(ctx, fd, buf, len, flags) |
			complete_with_value<size_t>(
				[](io_uring_cqe* cqe, auto&& receiver) noexcept {
					if (cqe->res >= 0) {
						ex::set_value(std::move(receiver), static_cast<size_t>(cqe->res));
					} else {
						ex::set_error(std::move(receiver), system_error(-cqe->res));
					}
				}
			);
	}
};

struct recv_t {
	auto operator()(uring& ctx, int fd, void* buf, size_t len, int flags = 0) const noexcept {
		return make_oneshot_sender<io_uring_prep_recv>(ctx, fd, buf, len, flags) |
			complete_with_value<size_t>(
				[](io_uring_cqe* cqe, auto&& receiver) noexcept {
					if (cqe->res >= 0) {
						ex::set_value(std::move(receiver), static_cast<size_t>(cqe->res));
					} else {
						ex::set_error(std::move(receiver), system_error(-cqe->res));
					}
				}
			);
	}
};

struct accept_loop_t {
	template <typename Func>
	auto operator()(uring& ctx, int fd, sockaddr *addr, socklen_t* addrlen, int flags, Func&& func) const noexcept {
		return make_multishot_sender<io_uring_prep_multishot_accept>(
			ctx,
			[cb = std::forward<Func>(func)](auto stop, io_uring_cqe* cqe) noexcept {
				if (cqe->res >= 0) {
					std::invoke(cb, stop, cqe->res);
				} else {
					std::invoke(cb, stop, std::unexpected(system_error(-cqe->res)));
				}
			},
			fd,
			addr,
			addrlen,
			flags
		);
	}

	template <typename Func>
	auto operator()(uring& ctx, int fd, sockaddr *addr, socklen_t* addrlen, Func&& func) const noexcept {
		return operator()(ctx, fd, addr, addrlen, 0, std::forward<Func>(func));
	}

	template <typename Func>
	auto operator()(uring& ctx, int fd, int flags, Func&& func) const noexcept {
		return operator()(ctx, fd, nullptr, 0, flags, std::forward<Func>(func));
	}

	template <typename Func>
	auto operator()(uring& ctx, int fd, Func&& func) const noexcept {
		return operator()(ctx, fd, nullptr, 0, 0, std::forward<Func>(func));
	}
};

} // namespace detail

constexpr inline auto bind    = detail::io_factory<detail::bind_t>{};
constexpr inline auto listen  = detail::io_factory<detail::listen_t>{};
constexpr inline auto accept  = detail::io_factory<detail::accept_t>{};
constexpr inline auto connect = detail::io_factory<detail::connect_t>{};
constexpr inline auto send    = detail::io_factory<detail::send_t>{};
constexpr inline auto recv    = detail::io_factory<detail::recv_t>{};

constexpr inline auto accept_loop = detail::io_factory<detail::accept_loop_t>{};

} // namespace uexec
