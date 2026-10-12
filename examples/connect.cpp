#include <print>
#include <thread>
#include <csignal>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <execution.hpp>

#include <uexec/uring.hpp>
#include <uexec/operation.hpp>

uexec::uring* stop_me = nullptr;

void handle_signal(int) {
	stop_me->request_stop();
}

#define debug(...) { try { std::println(__VA_ARGS__); } catch (...) {} }

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

	std::string msg = "hello world!";
	std::array<uint8_t, 20> input;

	uexec::uring uring(2048);
	stop_me = &uring;
	std::signal(SIGINT, handle_signal);

	uring.spawn(
		uexec::connect(sock, addr) |
		ex::then([] noexcept {
			debug("socket connected");
		}) |
		ex::let_error([](std::error_code err) noexcept {
			debug("connect failed: {}", err);
			return ex::just_stopped();
		}) |
		ex::let_value([sock, &msg] noexcept {
			return uexec::send(sock, msg.data(), msg.size());
		}) |
		ex::let_error([](std::error_code err) noexcept {
			debug("send failed: {}", err);
			return ex::just_stopped();
		}) |
		ex::let_value([sock, &input](size_t len) noexcept {
			debug("sent {} bytes", len);
			return uexec::recv(sock, input.data(), input.size());
		}) |
		ex::let_error([](std::error_code err) noexcept {
			debug("recv failed: {}", err);
			return ex::just_stopped();
		}) |
		ex::then([&input](size_t len) noexcept {
			auto str = std::string_view(reinterpret_cast<const char*>(input.data()), len);
			debug("recv msg: {}", str);
		})
	);

	uring.run_until_stopped();

	return 0;
}
