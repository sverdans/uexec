#include <print>
#include <thread>
#include <csignal>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <execution.hpp>

#include <uexec/uring.hpp>
#include <uexec/as_expected.hpp>

using namespace uexec;

int main() {
	uexec::uring uring(2048);

	uring.spawn([&uring]() -> ex::task<> {
		int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock == -1) {
			std::println("socket error: {}", std::strerror(errno));
			co_return;
		}

		sockaddr_in addr{};
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port        = htons(8888);

		auto res = co_await as_expected(uring.connect(sock, addr));
		if (not res) {
			std::println("connect failed: {}", res.error());
			co_return;
		}

		std::println("socket connected");
	});

	uring.spawn<exception_policy::abort>([&uring]() -> ex::task<> {
		auto res = co_await as_expected(uring.receive_signal(SIGINT));
		if (not res) {
			std::println("receive signal failed: {}", res.error());
			co_return;
		}
		std::println("SIGINT received");
		uring.request_stop();
	});

	uring.run_until_stopped();

	return 0;
}
