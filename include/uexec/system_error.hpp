#pragma once

#include <cerrno>
#include <system_error>
#include <format>

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

template <>
struct std::formatter<std::error_code> {
	template <class ParseContext>
	constexpr auto parse(ParseContext& ctx) {
		return ctx.begin();
	}

	template <class FormatContext>
	auto format(std::error_code err, FormatContext& ctx) const {
		return std::format_to(
			ctx.out(),
			"{}:{}({})",
			err.category().name(),
			err.value(),
			err.message()
		);
	}
};
