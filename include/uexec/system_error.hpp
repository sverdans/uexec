#pragma once

#include <cerrno>
#include <system_error>

namespace uexec {

inline std::error_code system_error(int err = errno) noexcept {
	return std::error_code(err, std::system_category());
}

inline void throw_system_error_if(bool condition, int err = errno) {
	if (condition) {
		throw std::system_error(system_error(err));
	}
}

} // namespace uexec
