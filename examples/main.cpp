#include <print>
#include <thread>
#include <csignal>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <execution.hpp>

#include <uexec/uring.hpp>

int main() {
	int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		std::println("socket error: {}", std::strerror(errno));
		return EXIT_FAILURE;
	}

	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port        = htons(8888);

	uexec::uring uring(2048);

	uring.spawn(
		uring.connect(sock, addr) |
		ex::then([](io_uring_cqe* cqe) {
			if (cqe->res >= 0) {
				std::println("socket connected");
			} else {
				std::println("connect failed: {}", uexec::system_error(-cqe->res).message());
			}
		}) |
		ex::upon_stopped([] {
			std::println("connect cancelled");
		}) |
		ex::upon_error([](auto err) noexcept {
			if constexpr (std::is_same_v<std::error_code, decltype(err)>) {
				std::println("connect failed in core: {}", err.message());
			}
		})
	);

	uring.spawn(
		uring.handle_signal(SIGINT) |
		ex::then([&uring](io_uring_cqe* cqe) {
			if (cqe->res >= 0) {
				std::println("handle_signal");
				uring.request_stop();
			} else {
				std::println("handle_signal failed: {}", uexec::system_error(-cqe->res).message());
			}
		}) |
		ex::upon_stopped([] {
			std::println("handle_signal cancelled");
		}) |
		ex::upon_error([](auto err) noexcept {
			if constexpr (std::is_same_v<std::error_code, decltype(err)>) {
				std::println("handle_signal failed in core: {}", err.message());
			}
		})
	);

	uring.run_until_stopped();

	return 0;
}
