#pragma once

#include <tuple>
#include <type_traits>
#include <variant>

#include <execution.hpp>

namespace uexec {
namespace detail {

template <class V>
struct unpacked_sender_value {};

template <class V>
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

template <class Sender, class Env = ex::env<>>
struct sender_value {
	using type =
		typename detail::unpacked_sender_value<
			ex::value_types_of_t<Sender, Env>
		>::type;
};

template <class Sender, class Env = ex::env<>>
using sender_value_t = sender_value<Sender, Env>::type;

} // namespace uexec
