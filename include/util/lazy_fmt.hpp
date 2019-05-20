#pragma once

#include <string>
#include <functional> // std::invoke
#include <type_traits> // std::is_lvalue_reference, std::enable_if
#include <fmt/format.h>

namespace waybar::lazy {

template <typename FuncT>
struct arg {
  explicit arg(FuncT&& f_) noexcept : f(f_) {};
  explicit arg(const FuncT& f_) noexcept : f(f_) {};

  mutable FuncT f;
};

template <typename FuncT>
auto make(FuncT&& f) {
  return arg{std::forward<FuncT>(f)};
}

template <typename FuncT, typename Arg1, typename... Args>
auto make(fmt::string_view name, FuncT&& f, Arg1&& arg1, Args&&... args) {
  return arg{[&] () mutable -> decltype(auto) { return f(std::forward<Arg1>(arg1), std::forward<Args>(args)...); }};
}

template <typename Type, typename ReturnType, typename... Args>
auto make(fmt::string_view name, Type& this_, ReturnType (Type::*f)(Args...), Args&&... args) {
  return arg{[&] () mutable -> ReturnType { return std::invoke(f, this_, std::forward<Args>(args)...); }};
}

template <typename Type, typename ReturnType, typename... Args>
auto make(fmt::string_view name, Type* this_, ReturnType (Type::*f)(Args...), Args&&... args) {
  return make(*this_, f, std::forward<Args>(args)...);
}
}



namespace fmt {
template <typename FuncT>
struct formatter<waybar::lazy::arg<FuncT>> {
  template <typename ParseContext>
  constexpr auto parse(ParseContext &ctx) {
    tm_format.clear();

    auto end = std::find(ctx.begin(), ctx.end(), '}');
    tm_format.reserve(static_cast<unsigned>(end - ctx.begin()) + 3);
    tm_format += '{';
    tm_format += ':';
    tm_format.append(ctx.begin(), end);
    tm_format += '}';

    return end;
  }

  template <typename FormatContext>
  auto format(const waybar::lazy::arg<FuncT> &lazy, FormatContext &ctx) {
    return format_to(ctx.out(), tm_format, lazy.f());
  }

  std::string tm_format;
};
}

