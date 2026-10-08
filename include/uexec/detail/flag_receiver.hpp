#pragma once

#include <execution.hpp>

namespace uexec {
namespace detail {

template <typename Scheduler>
class flag_receiver {
public:
	using receiver_concept = ex::receiver_tag;

	flag_receiver(Scheduler sched, bool& flag) noexcept
		: _sched(sched)
		, _flag(&flag) {}

public:
	template <class E>
	void set_error(E&&) noexcept { deed_done(); }

	void set_value() noexcept { deed_done(); }

	void set_stopped() noexcept { deed_done(); }

	auto get_env() const noexcept {
		return ex::env{
			ex::prop{ex::get_start_scheduler, _sched}
		};
	}

private:
	void deed_done() noexcept { *_flag = true; }

private:
	Scheduler _sched;
	bool* _flag;
};

} // namespace uexec
} // namespace detail
