#pragma once

#include <tuple>
#include <type_traits>
#include <variant>

#include <execution.hpp>

namespace uexec {
namespace detail {

template <typename V>
struct unpacked_sender_value {};

template <typename V>
struct unpacked_sender_value<std::variant<std::tuple<V>>> {
	using type = V;
};

template <>
struct unpacked_sender_value<std::variant<std::tuple<>>> {
	using type = void;
};

} // namespace detail

template <ex::sender Sender, ex::receiver Receiver>
using connect_type = decltype(ex::connect(std::declval<Sender>(), std::declval<Receiver>()));

template <typename Sender, typename Env = ex::env<>>
struct sender_value {
	using type =
		typename detail::unpacked_sender_value<
			ex::value_types_of_t<Sender, Env>
		>::type;
};

template <typename Sender, typename Env = ex::env<>>
using sender_value_t = sender_value<Sender, Env>::type;

template <typename Receiver>
using stop_token_t = ex::stop_token_of_t<ex::env_of_t<Receiver>>;

} // namespace uexec
