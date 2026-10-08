#pragma once

#include <utility>
#include <exception>

#include <execution.hpp>

namespace uexec {

// TODO
//   - переосмыслить это

enum class exception_policy {
	deny,
	abort,
	standard = abort,
};

template <ex::sender Sender>
ex::sender auto with_abort_on_exception(Sender&& sndr) {
	return std::move(sndr) | ex::upon_error([](std::exception_ptr err) noexcept {
		try {
			std::rethrow_exception(err);
		} catch (const std::exception& exception) {
			std::fputs(exception.what(), stderr);
			std::abort();
		} catch (...) {
			std::fputs("unknown exception", stderr);
			std::abort();
		}
	});
}

} // namespace uexec
