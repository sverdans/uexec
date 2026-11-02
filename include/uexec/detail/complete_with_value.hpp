#pragma once

#include <execution.hpp>

namespace uexec {
namespace detail {

template <typename V>
struct complete_with_value_signatures {
	using type = ex::completion_signatures<
		ex::set_value_t(V),
		ex::set_error_t(std::error_code),
		ex::set_stopped_t()
	>;
};

template <>
struct complete_with_value_signatures<void> {
	using type = ex::completion_signatures<
		ex::set_value_t(),
		ex::set_error_t(std::error_code),
		ex::set_stopped_t()
	>;
};

template <typename V>
using complete_with_value_signatures_t = typename complete_with_value_signatures<V>::type;

template <typename V, typename Sender, typename Func>
class complete_with_value_sender
	: public ex::sender_adaptor_closure<complete_with_value_sender<V, Sender, Func>> {
public:
	using sender_concept = ex::sender_tag;

	template <class Self, class Env>
	static consteval auto get_completion_signatures() noexcept {
		return complete_with_value_signatures_t<V>{};
	}

	template <typename Receiver>
	class receiver {
	public:
		using receiver_concept = ex::receiver_tag;

		receiver(Receiver&& rcvr, Func&& func) noexcept
			: _rcvr(std::move(rcvr))
			, _func(std::move(func)) {}

	public:
		auto get_env() const noexcept {
			return ex::get_env(_rcvr);
		}

		void set_value(io_uring_cqe* cqe) noexcept {
			std::invoke(_func, cqe, std::move(_rcvr));
		}

		void set_error(std::error_code&& error) noexcept {
			ex::set_error(std::move(_rcvr), std::move(error));
		}

		void set_stopped() noexcept {
			ex::set_stopped(std::move(_rcvr));
		}

	private:
		Receiver _rcvr;
		[[no_unique_address]] Func _func;
	};

	complete_with_value_sender(Sender&& sndr, Func&& func)
		: _sndr(std::move(sndr))
		, _func(std::move(func)) {}

public:
	template <ex::receiver Receiver>
	auto connect(Receiver&& rcvr) noexcept {
		using underlying_receiver = std::remove_cvref_t<Receiver>;
		using transformed_receiver = receiver<underlying_receiver>;
		return ex::connect(
			std::move(_sndr),
			transformed_receiver(
				std::move(rcvr),
				std::move(_func)
			)
		);
	}

private:
	Sender _sndr;
	[[no_unique_address]] Func _func;
};

template <typename V, typename Func>
struct complete_with_value_closure
	: public ex::sender_adaptor_closure<complete_with_value_closure<V, Func>> {

	explicit complete_with_value_closure(Func function)
		: func(std::move(function)) {}

	template <ex::sender Sender>
	auto operator()(Sender&& sndr) && {
		using underlying_sender = std::remove_cvref_t<Sender>;
		using sender_t = complete_with_value_sender<V, underlying_sender, Func>;
		return sender_t(
			std::forward<Sender>(sndr),
			std::move(func)
		);
	}

	[[no_unique_address]] Func func;
};

template <typename V, typename Func>
auto complete_with_value(Func&& func) {
	using function_t = std::decay_t<Func>;
	return complete_with_value_closure<V, function_t>(std::forward<Func>(func));
}

} // namespace detail
} // namespace uexec
