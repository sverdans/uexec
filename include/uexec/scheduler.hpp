#pragma once

#include <execution.hpp>

namespace uexec {

// Планировщик, ассоциированный с контекстом исполнения.
// Операция стартует синхронно в вызывающем потоке.

// TODO:
// - проверить возможность остановки

template <typename Context>
class eager_scheduler {
public:
	eager_scheduler(Context& ctx) noexcept : _ctx(ctx) {}

	class env {
	public:
		env(Context& ctx) noexcept : _ctx(ctx) {}

		auto query(ex::get_completion_scheduler_t<ex::set_value_t>) const noexcept {
			return _ctx.get_scheduler();
		}

	private:
		Context& _ctx;
	};

	template <ex::receiver Receiver>
	class operation {
	public:
		operation(Context& ctx, Receiver&& rcvr) noexcept
			: _ctx(ctx)
			, _rcvr(std::move(rcvr)) {}

		void start() noexcept {
			ex::set_value(std::move(_rcvr));
		}

	private:
		Context& _ctx;
		Receiver _rcvr;
	};

	class sender {
	public:
		using sender_concept = ex::sender_tag;

		template <class Self, class Env>
		static consteval auto get_completion_signatures() noexcept {
			return ex::completion_signatures<ex::set_value_t()>{};
		}

		explicit sender(Context& ctx) noexcept : _ctx(ctx) {}

		env get_env() const noexcept { return env(_ctx); }

		template <class Receiver>
		auto connect(Receiver rcvr) const {
			return operation<Receiver>(_ctx, std::move(rcvr));
		}

	private:
		Context& _ctx;
	};

public:
	Context& context() noexcept { return _ctx; }

	sender schedule() const noexcept { return sender(_ctx); }

	bool operator==(const eager_scheduler& other) const noexcept {
		return &_ctx == &other._ctx;
	}

private:
	Context& _ctx;
};

} // namespace uexec
