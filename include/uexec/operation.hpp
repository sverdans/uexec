#pragma once

#include <netinet/in.h>

#include <execution.hpp>

#include <uexec/system_error.hpp>
#include <uexec/detail/oneshot_sender.hpp>
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

} // namespace detail

constexpr inline auto connect = detail::io_factory<detail::connect_t>{};
constexpr inline auto send    = detail::io_factory<detail::send_t>{};
constexpr inline auto recv    = detail::io_factory<detail::recv_t>{};

} // namespace uexec
