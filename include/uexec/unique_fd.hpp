#pragma once

#include <utility>

#include <unistd.h>

namespace uexec {

class unique_fd final {
public:
	static constexpr int bad = -1;

	explicit unique_fd(int fd) noexcept : _fd(fd) {}

	unique_fd(const unique_fd&) = delete;

	unique_fd& operator=(const unique_fd& other) = delete;

	unique_fd(unique_fd&& other) noexcept : _fd(other.release()) {}

	unique_fd& operator=(unique_fd&& other) noexcept {
		unique_fd temp(std::move(other));
		std::swap(_fd, temp._fd);
		return *this;
	}

	~unique_fd() { close(); }

public:
	explicit operator int() const noexcept { return _fd; }

	explicit operator bool() const noexcept { return is_valid(); }

	int get() const noexcept { return _fd; }

	int release() noexcept { return std::exchange(_fd, bad); }

	void reset(int fd = bad) noexcept {
		close();
		_fd = fd;
	}

private:
	bool is_valid() const noexcept { return _fd > bad; }

	void close() noexcept {
		if (is_valid()) {
			::close(_fd);
		}
	}

private:
	int _fd = bad;
};

} // namespace uexec
