#include <print>
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

int main() {
	using uexec::as_expected;

	uexec::uring uring(2048);
	stop_me = &uring;
	std::signal(SIGINT, handle_signal);

	uring.spawn([&uring]() -> ex::task<> {
		uexec::unique_fd sock{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
		if (not sock) {
			std::println("socket error: {}", std::strerror(errno));
			co_return;
		}

		sockaddr_in addr{};
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port        = htons(8888);

		auto res = co_await as_expected(uexec::connect(uring, sock.get(), addr));
		if (not res) {
			std::println("connect failed: {}", res.error());
			co_return;
		}

		std::println("socket connected");

		std::string msg = "hello world!";
		auto sent = co_await as_expected(uexec::send(uring, sock.get(), msg.data(), msg.size()));
		if (not sent) {
			std::println("send failed: {}", sent.error());
			co_return;
		}

		std::println("sent {} bytes", *sent);

		std::array<uint8_t, 20> input;
		auto received = co_await as_expected(uexec::recv(uring, sock.get(), input.data(), input.size()));
		if (not received) {
			std::println("recv failed: {}", received.error());
			co_return;
		}

		auto str = std::string_view(reinterpret_cast<const char*>(input.data()), *received);
		std::println("recv msg: {}", str);
	});

	uring.run_until_stopped();

	return 0;
}
