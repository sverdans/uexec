#include <print>

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
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_LOOPBACK;
	addr.sin_port = htons(8888);

	uexec::uring uring(2048);

	ex::run_loop loop;
	auto chain =
		ex::schedule(loop.get_scheduler()) |
		uexec::connect(sock, addr) |

	return 0;
}
