#pragma once

#include <expected>
#include <type_traits>

#include <execution.hpp>

#include <uexec/meta.hpp>

namespace uexec {
namespace detail {

// TODO:
//   - вывод типов и forwarding в ctr
//   - exception safety
//   - проеврка error_type оборачиваемого сендера
//   - зависимость expected_type от Env
//         - template <typename Env> value_type = sender_value_t<sender_type, Env>;
//         - template <typename Env> expected_type = std::expected<value_type<Env>, error_type>;

template <ex::sender Sender>
class as_expected_sender {
public:
	using sender_concept = ex::sender_tag;
	using value_type     = sender_value_t<Sender>;
	using error_type     = std::error_code;
	using expected_type  = std::expected<value_type, error_type>;

public:

	template <class Self, class Env>
	static consteval auto get_completion_signatures() noexcept {
		return ex::completion_signatures<
			ex::set_value_t(expected_type),
			ex::set_stopped_t()
		>{};
	}

	template <typename Receiver>
	struct receiver {
		using receiver_concept = ex::receiver_tag;

		template <typename... Vs>
		void set_value(Vs&&... vs) noexcept {
			ex::set_value(
				std::move(rcvr),
				expected_type(
					std::in_place,
					std::forward<Vs>(vs)...
				)
			);
		}

		void set_error(error_type error) noexcept {
			ex::set_value(
				std::move(rcvr),
				expected_type(
					std::unexpected(
						std::move(error)
					)
				)
			);
		}

		void set_stopped() noexcept { ex::set_stopped(std::move(rcvr)); }

		auto get_env() const noexcept { return ex::get_env(rcvr); }

		Receiver rcvr;
	};

	explicit as_expected_sender(Sender&& sndr) noexcept
		: _sndr(std::move(sndr)) {}

	template <ex::receiver Receiver>
	auto connect(Receiver&& rcvr) {
		using underlying_receiver = std::remove_cvref_t<Receiver>;
		using transformed_receiver = receiver<underlying_receiver>;
		return ex::connect(
			std::move(_sndr),
			transformed_receiver(std::forward<Receiver>(rcvr))
		);
	}

private:
	Sender _sndr;
};

} // namespace detail

template <ex::sender Sender>
ex::sender auto as_expected(Sender&& sndr) noexcept {
	using sender_type = std::remove_cvref_t<Sender>;
	return detail::as_expected_sender<sender_type>(std::forward<Sender>(sndr));
}

} // namespace uexec
