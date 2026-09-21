#pragma once

#include <span>
#include <utility>

#include <sys/mman.h>

#include <uexec/types.hpp>
#include <uexec/system_error.hpp>

namespace uexec {

class mapped_memory final {
private:
	static void* map_memory(size_t size) {
		constexpr int prot  = PROT_READ | PROT_WRITE;
		constexpr int flags = MAP_PRIVATE | MAP_ANONYMOUS;
		constexpr int fd    = -1;
		void* ptr = ::mmap(nullptr, size, prot, flags, fd, 0);
		throw_system_error_if(ptr == MAP_FAILED);
		return ptr;
	}

public:
	mapped_memory(size_t size)
		: _storage(map_memory(size))
		, _size(size) {}

	mapped_memory(const mapped_memory&) = delete;

	mapped_memory& operator=(const mapped_memory&) = delete;

	mapped_memory(mapped_memory&& other) noexcept
		: _storage(std::exchange(other._storage, nullptr))
		, _size(std::exchange(other._size, 0)) {}

	mapped_memory& operator=(mapped_memory&& other) noexcept {
		std::swap(_storage, other._storage);
		std::swap(_size, other._size);
		return *this;
	}

	~mapped_memory() {
		if (_storage) {
			::munmap(_storage, _size);
		}
	}

public:
	size_t size() const noexcept { return _size; }

	void* data() noexcept { return _storage; }

	const void* data() const noexcept { return _storage; }

private:
	void* _storage = nullptr;
	size_t _size = 0;
};

} // namespace uexec
