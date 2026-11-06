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

class echo_session {
public:
	echo_session(int sock) noexcept : _sock(sock) {}

	~echo_session() { debug("session closed"); }

	auto run() & noexcept {
		return
			uexec::recv(_sock.get(), buf.data(), buf.size()) |
			ex::let_value([this](size_t len) noexcept {
				auto str = std::string_view(reinterpret_cast<const char*>(buf.data()), len);
				debug("recv msg: {}", str);
				return uexec::send(_sock.get(), buf.data(), len);
			}) |
			ex::then([](size_t len) noexcept {
				debug("sent {} bytes", len);
			}) |
			ex::upon_error([](std::error_code err) noexcept {
				debug("stopped with error: {}", err);
			}) |
			ex::upon_stopped([] noexcept {
				debug("cancelled");
			});
		}
private:
	uexec::unique_fd _sock;
	std::array<uint8_t, 4096> buf;
};

auto serve(int sock) {
	return ex::just(std::make_unique<echo_session>(sock)) |
		ex::let_value([](std::unique_ptr<echo_session>& session) noexcept {
			return session->run();
		});
}

auto listen_ipv4(int fd, uint32_t hostlong, uint16_t hostport, int backlog) noexcept {
	sockaddr_in addr{};
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(hostlong);
	addr.sin_port        = htons(hostport);
	return ex::just(addr) |
		ex::let_value([fd](sockaddr_in& addr) noexcept {
			return uexec::bind(fd, reinterpret_cast<sockaddr*>(&addr), socklen_t(sizeof(addr)));
		}) |
		ex::let_value([fd, backlog] noexcept {
			return uexec::listen(fd, backlog);
		});
}

auto accept_clients(uexec::uring& ctx, int listener) noexcept {
	return listen_ipv4(listener, INADDR_LOOPBACK, 8888, 1024) |
		ex::let_value(
			[&ctx, listener]() noexcept {
				debug("server started on 127.0.0.1:8888");
				return uexec::accept_loop(listener,
					[&ctx](auto stop, std::expected<int, std::error_code> result) noexcept {
						if (result) {
							debug("client connected");
							ctx.spawn(serve(*result));
						} else {
							debug("accept failed: {}", result.error());
							stop.request_stop();
						}
					}
				);
			}
		);
}

int main() {
	uexec::unique_fd sock{socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
	if (not sock) {
		std::println("socket error: {}", std::strerror(errno));
		return EXIT_FAILURE;
	}

	uexec::uring uring(2048);

	uring.spawn(
		accept_clients(uring, sock.get()) |
		ex::upon_error([](std::error_code error) noexcept {
			debug("server failed: {}", error);
		}) |
		ex::upon_stopped([]() noexcept {
			debug("server stopped");
		})
	);

	uring.run_until_stopped();
	return 0;
}
