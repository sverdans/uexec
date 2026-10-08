#pragma once

#include <expected>
#include <type_traits>

#include <execution.hpp>

#include <uexec/meta.hpp>

namespace uexec {

// TODO:
//   - вывод типов и forwarding в ctr
//   - exception safety
//   - проеврка error_type оборачиваемого сендера

template <ex::sender Sender>
class as_expected_sender {
public:
	using sender_concept = ex::sender_tag;
	using sender_type    = std::remove_cvref_t<Sender>;
	using value_type     = sender_value_t<sender_type>;
	using error_type     = std::error_code;
	using expected_type  = std::expected<value_type, error_type>;

public:
	template <ex::receiver Receiver>
	class operation_state {
	public:
		using receiver_type = std::remove_cvref_t<Receiver>;

		struct receiver {
		public:
			using receiver_concept = ex::receiver_tag;

			template <class... Vs>
			void set_value(Vs&&... vs) noexcept {
				if constexpr (sizeof...(Vs) == 0) {
					ex::set_value(std::move(rcvr), expected_type(std::in_place));
				} else {
					ex::set_value(std::move(rcvr), expected_type(std::in_place, std::forward<Vs>(vs)...));
				}
			}

			void set_error(error_type&& error) noexcept {
				ex::set_value(std::move(rcvr), expected_type(std::unexpected(std::move(error))));
			}

			void set_stopped() noexcept {
				ex::set_stopped(std::move(rcvr));
			}

			auto get_env() const noexcept {
				return ex::get_env(rcvr);
			}

			receiver_type rcvr;
		};

		using inner_type = connect_type<sender_type, receiver>;

		operation_state(sender_type&& sndr, receiver_type&& rcvr)
			: _operation(ex::connect(std::move(sndr), receiver(std::move(rcvr)))) {}

		void start() noexcept {
			ex::start(_operation);
		}

	private:
		inner_type _operation;
	};

	template <class Self, class Env>
	static consteval auto get_completion_signatures() noexcept {
		return ex::completion_signatures<
			ex::set_value_t(expected_type),
			ex::set_stopped_t()
		>{};
	}

	explicit as_expected_sender(sender_type&& sndr) noexcept(std::is_nothrow_move_constructible_v<sender_type>)
		: _sndr(std::move(sndr)) {}

	template <ex::receiver Receiver>
	auto connect(Receiver&& rcvr) {
		return operation_state<Receiver>(std::move(_sndr), std::move(rcvr));
	}

private:
	sender_type _sndr;
};

template <ex::sender Sender>
ex::sender auto as_expected(Sender&& sndr) {
	return as_expected_sender<Sender>(std::move(sndr));
}

} // namespace uexec
