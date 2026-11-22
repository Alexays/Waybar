#pragma once

#include <fmt/format.h>

#include <concepts>
#include <type_traits>

namespace waybar::util {

// Wrap a getter to evaluate it each time fmt renders the field. For example,
// LazyFormatArg{[&text] { return text.size(); }} formats a string's length.
// Ensure you keep captured references alive until formatting finishes.
template <std::invocable Getter>
struct LazyFormatArg {
  using Value = std::remove_cvref_t<std::invoke_result_t<const Getter&>>;

  Getter getter_;
};

}  // namespace waybar::util

template <std::invocable Getter>
struct fmt::formatter<waybar::util::LazyFormatArg<Getter>>
    : fmt::formatter<typename waybar::util::LazyFormatArg<Getter>::Value> {
  using Arg = waybar::util::LazyFormatArg<Getter>;
  using Base = fmt::formatter<typename Arg::Value>;

  auto format(const Arg& arg, auto& ctx) const { return Base::format(arg.getter_(), ctx); }
};
