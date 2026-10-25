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

#define debug(...) { try { std::println(__VA_ARGS__); } catch (...) {} }

uexec::uring* stop_me = nullptr;

void handle_signal(int) {
	stop_me->request_stop();
}

auto connect_ipv4(int fd, uint32_t hostlong, uint16_t hostport) {
	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(hostlong);
	addr.sin_port        = htons(hostport);
	return ex::just(addr) |
		ex::let_value([fd](sockaddr_in& addr) noexcept {
			return uexec::connect(fd, reinterpret_cast<sockaddr*>(&addr), socklen_t(sizeof(addr)));
		});
}

auto send_message(int fd, std::string&& msg) {
	return ex::just(std::move(msg)) |
		ex::let_value([fd](std::string& msg) noexcept {
			return uexec::send(fd, msg.data(), msg.size());
		});
}

int main() {
	uexec::unique_fd sock{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
	if (not sock) {
		std::println("socket error: {}", std::strerror(errno));
		return EXIT_FAILURE;
	}

	std::array<uint8_t, 20> input;
	uexec::uring uring(2048);
	stop_me = &uring;
	std::signal(SIGINT, handle_signal);

	uring.spawn(
		ex::just(sock.get()) |
		ex::let_value([](int sock) noexcept {
			return connect_ipv4(sock, INADDR_LOOPBACK, 8888);
		}) |
		ex::let_error([](std::error_code err) noexcept {
			debug("connect failed: {}", err);
			return ex::just_stopped();
		}) |
		ex::let_value([&] noexcept {
			debug("socket connected");
			return send_message(sock.get(), "hello world");
		}) |
		ex::let_error([](std::error_code err) noexcept {
			debug("send failed: {}", err);
			return ex::just_stopped();
		}) |
		ex::let_value([&](size_t len) noexcept {
			debug("sent {} bytes", len);
			return uexec::recv(sock.get(), input.data(), input.size());
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
