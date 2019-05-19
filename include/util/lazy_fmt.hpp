#pragma once

#include <string>
#include <fmt/format.h>

namespace waybar {
template <typename FuncT>
struct lazy_fmt {
  explicit lazy_fmt(FuncT&& f_) noexcept : f{f_} {};
  explicit lazy_fmt(FuncT& f_) noexcept : f{f_} {};
  FuncT f;
};

template <typename FuncT>
auto lazy_arg(fmt::string_view name, FuncT&& f) {
  return fmt::arg(name, lazy_fmt{std::forward<FuncT>(f)});
}
}



namespace fmt {
template <typename T>
struct formatter<waybar::lazy_fmt<T>> {
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

  template <typename FuncT, typename FormatContext>
  auto format(const waybar::lazy_fmt<FuncT> &lazy, FormatContext &ctx) {
    return format_to(ctx.out(), tm_format, lazy.f());
  }

  std::string tm_format;
};
}

