#include <cstdlib>
#include <print>
#include <exec/task.hpp>
#include <thread>
#include <csignal>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <execution.hpp>

#include <uexec/uring.hpp>
#include <uexec/unique_fd.hpp>
#include <uexec/operation.hpp>
#include <uexec/as_expected.hpp>

uexec::uring* stop_me = nullptr;

void handle_signal(int) {
	stop_me->request_stop();
}

ex::task<void> make_session(uexec::uring& uring, uint32_t host, uint16_t port) {
	uexec::unique_fd sock{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
	if (not sock) {
		std::println("socket error: {}", std::strerror(errno));
		co_return;
	}

	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(host);
	addr.sin_port        = htons(port);

	auto res = co_await uexec::as_expected(uexec::connect(
		uring,
		sock.get(),
		reinterpret_cast<sockaddr*>(&addr),
		socklen_t(sizeof(addr))
	));

	if (not res) {
		std::println("connect failed: {}", res.error());
		co_return;
	}

	std::println("socket connected");

	std::string msg = "hello world!";
	auto sent = co_await uexec::as_expected(uexec::send(uring, sock.get(), msg.data(), msg.size()));
	if (not sent) {
		std::println("send failed: {}", sent.error());
		co_return;
	}

	std::println("sent {} bytes", *sent);

	std::array<uint8_t, 20> input;
	auto received = co_await uexec::as_expected(uexec::recv(uring, sock.get(), input.data(), input.size()));
	if (not received) {
		std::println("recv failed: {}", received.error());
		co_return;
	}

	auto str = std::string_view(reinterpret_cast<const char*>(input.data()), *received);
	std::println("recv msg: {}", str);
}

auto session(uexec::uring& uring, uint32_t host, uint16_t port) {
	return ex::let_value(ex::just(), [&uring, host, port] {
			return make_session(uring, host, port);
		}) |
		ex::upon_error([](std::exception_ptr) noexcept {
			std::abort();
		});
}

int main() {
	uexec::uring uring(2048);
	stop_me = &uring;
	std::signal(SIGINT, handle_signal);

	uring.spawn(session(uring, INADDR_LOOPBACK, 8888));
	uring.run_until_stopped();
	return 0;
}
