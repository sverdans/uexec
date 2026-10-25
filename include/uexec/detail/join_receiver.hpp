#pragma once

#include <execution.hpp>

namespace uexec {
namespace detail {

template <class Scheduler>
class join_receiver final {
public:
	using receiver_concept = ex::receiver_tag;

	join_receiver(Scheduler scheduler, bool& joined) noexcept
		: _scheduler(scheduler)
		, _joined(joined) {}

	void set_value() noexcept { _joined = true; }

	auto get_env() const noexcept {
		return ex::env{
			ex::prop{ex::get_start_scheduler, _scheduler}
		};
	}

private:
	Scheduler _scheduler;
	bool& _joined;
};

} // namespace uexec
} // namespace detail
