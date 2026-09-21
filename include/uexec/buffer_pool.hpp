#pragma once

#include <bit>
#include <cassert>
#include <span>

#include <liburing.h>

#include <uexec/types.hpp>
#include <uexec/mapped_memory.hpp>

namespace uexec {

class buffer_ring final {
	static size_t ring_bytes_count(size_t count) {
		if (not std::has_single_bit(count)) {
			throw std::invalid_argument("buffer count must be power of two");
		}
		return sizeof(io_uring_buf_ring) + (sizeof(io_uring_buf) * count);
	}

public:
	buffer_ring(size_t count)
		: _ring_storage(ring_bytes_count(count))
		, _count(count) {
		io_uring_buf_ring_init(ring());
	}

	size_t count() const noexcept { return _count; }

	int mask() const noexcept {
		return io_uring_buf_ring_mask(static_cast<unsigned>(_count));
	}

	void add(void* addr, unsigned len, u16 bid, unsigned offset) noexcept {
		io_uring_buf_ring_add(ring(), addr, len, bid, mask(), static_cast<int>(offset));
	}

	void advance(size_t count) noexcept {
		io_uring_buf_ring_advance(ring(), static_cast<int>(count));
	}

private:
	io_uring_buf_ring* ring() noexcept {
		return static_cast<io_uring_buf_ring*>(_ring_storage.data());
	}

private:
	mapped_memory _ring_storage;
	size_t _count;
};

class buffer_pool final {
public:
	buffer_pool(size_t count, size_t size)
		: _ring(count)
		, _mem(count * size)
		, _buff_size(size) { init(); }

public:
	size_t count() const noexcept { return _ring.count(); }

private:
	void init() noexcept {
		auto len = static_cast<unsigned>(_buff_size);
		for (u16 i = 0; i < _ring.count(); ++i) {
			u8* data = static_cast<u8*>(_mem.data()) + (i * _buff_size);
			_ring.add(data, len, i, i);
		}
		_ring.advance(_ring.count());
	}

private:
	buffer_ring _ring;
	mapped_memory _mem;
	size_t _buff_size;
};

} // namespace uexec
