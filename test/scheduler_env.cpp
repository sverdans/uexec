#include <thread>
#include <type_traits>

#include <execution.hpp>

#include <gtest/gtest.h>

#include <uexec/uring.hpp>

namespace {

template <typename Func>
auto get_start_scheduler(Func&& f) {
	return ex::let_value([] noexcept {
		return ex::read_env(ex::get_start_scheduler);
	}) |
	ex::then([func = std::forward<Func>(f)](auto sched) noexcept {
		func(sched);
	});
}

class st_context {
public:
	st_context() : _thread([this] { _loop.run(); }) {}

	~st_context() { _loop.finish(); }

	auto get_scheduler() noexcept { return _loop.get_scheduler(); }

private:
	ex::run_loop _loop;
	std::jthread _thread;
};

TEST(EnvPropagation, GetContextAfterSchedule) {
	uexec::uring ctx(2048);
	uexec::uring::scheduler sched = ctx.get_scheduler();

	using expected_sched_t = uexec::uring::scheduler;

	ex::sender auto sndr = ex::schedule(sched) |
		get_start_scheduler([&ctx](auto sched) noexcept {
			static_assert(std::is_same_v<expected_sched_t, decltype(sched)>);
			ASSERT_EQ(&sched.context(), &ctx);
		});

	ex::sync_wait(std::move(sndr));
}

TEST(EnvPropagation, GetContextAfterStartsOn) {
	uexec::uring ctx(2048);
	uexec::uring::scheduler sched = ctx.get_scheduler();

	using expected_sched_t = uexec::uring::scheduler;

	ex::sender auto sndr = ex::just() |
		get_start_scheduler([&ctx](auto sched) noexcept {
			static_assert(std::is_same_v<expected_sched_t, decltype(sched)>);
			ASSERT_EQ(&sched.context(), &ctx);
		}
	);

	ex::sync_wait(ex::starts_on(sched, std::move(sndr)));
}

TEST(EnvPropagation, GetContextAfterContinuesOn) {
	st_context cpu_ctx;
	auto cpu_sched = cpu_ctx.get_scheduler();

	uexec::uring io_ctx(2048);
	uexec::uring::scheduler io_sched = io_ctx.get_scheduler();

	using io_sched_t = uexec::uring::scheduler;
	using cpu_sched_t = decltype(cpu_sched);

	ex::sender auto sndr1 = ex::schedule(cpu_sched) |
		get_start_scheduler([](auto sched) noexcept {
			static_assert(std::is_same_v<decltype(sched), cpu_sched_t>);
		}) |
		ex::continues_on(io_sched) |
		get_start_scheduler([&io_ctx](auto sched) noexcept {
			static_assert(std::is_same_v<decltype(sched), io_sched_t>);
			ASSERT_EQ(&sched.context(), &io_ctx);
		});

	ex::sync_wait(std::move(sndr1));

	ex::sender auto sndr2 = ex::schedule(io_sched) |
		get_start_scheduler([&io_ctx](auto sched) noexcept {
			static_assert(std::is_same_v<decltype(sched), io_sched_t>);
			ASSERT_EQ(&sched.context(), &io_ctx);
		}) |
		ex::continues_on(cpu_sched) |
		get_start_scheduler([&](auto sched) noexcept {
			static_assert(std::is_same_v<decltype(sched), cpu_sched_t>);
		});

	ex::sync_wait(std::move(sndr2));
}

} // namespace
