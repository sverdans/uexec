#include <print>
#include <thread>
#include <csignal>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <execution.hpp>

#include <uexec/uring.hpp>
#include <uexec/operation.hpp>
#include <uexec/signal.hpp>

uexec::uring* stop_me = nullptr;

void handle_signal(int) {
	stop_me->request_stop();
}

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
	stop_me = &uring;
	std::signal(SIGINT, handle_signal);

	uring.spawn(
		uexec::connect(sock, addr) |
		ex::then([] noexcept {
			std::println("socket connected");
		}) |
		ex::upon_error([](std::error_code err) noexcept {
			std::println("connect failed: {}", err);
		}) |
		ex::upon_stopped([] noexcept {
			std::println("connect cancelled");
		}) |
		ex::upon_error([](auto err) noexcept {
			if constexpr (std::is_same_v<std::error_code, decltype(err)>) {
				std::println("connect failed: {}", err.message());
			}
		})
	);

	uring.run_until_stopped();

	return 0;
}
