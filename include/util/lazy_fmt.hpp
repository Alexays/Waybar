#pragma once

#include <string>
#include <functional> // std::invoke
#include <type_traits> // std::is_lvalue_reference, std::enable_if
#include <fmt/format.h>

namespace waybar::lazy {

template <typename FuncT>
struct anon_arg {
  explicit anon_arg(FuncT&& f_) noexcept : f(f_) {};
  explicit anon_arg(const FuncT& f_) noexcept : f(f_) {};

  mutable FuncT f;
};

template <typename FuncT, typename Char>
struct named_arg : fmt::internal::named_arg_base<Char> {
  using base = fmt::internal::named_arg_base<Char>;

  named_arg(fmt::basic_string_view<Char> name, FuncT&& value_) : base(name), value{std::move(value_)} {};
  named_arg(fmt::basic_string_view<Char> name, const FuncT& value_) : base(name), value{value_} {};

  const base &as_named_arg() {
    return *this;
  }

  mutable FuncT value;
};

template <typename FuncT>
auto arg(fmt::string_view name, FuncT&& f) {
  return named_arg{name, std::forward<FuncT>(f)};
}

template <typename FuncT, typename Arg1, typename... Args>
auto arg(fmt::string_view name, FuncT&& f, Arg1&& arg1, Args&&... args) {
  return named_arg{name, [&] () mutable -> decltype(auto) { return f(std::forward<Arg1>(arg1), std::forward<Args>(args)...); }};
}

template <typename Type, typename ReturnType, typename... Args>
auto arg(fmt::string_view name, Type& this_, ReturnType (Type::*f)(Args...), Args&&... args) {
  return named_arg{name, [&] () mutable -> ReturnType { return std::invoke(f, this_, std::forward<Args>(args)...); }};
}

template <typename Type, typename ReturnType, typename... Args>
auto arg(fmt::string_view name, Type* this_, ReturnType (Type::*f)(Args...), Args&&... args) {
  return arg(name, *this_, f, std::forward<Args>(args)...);
}
}



namespace fmt {

namespace internal {

template <typename C, typename T>
init<C, const waybar::lazy::named_arg<T, typename C::char_type>&, named_arg_type> make_value(
    const waybar::lazy::named_arg<T, typename C::char_type>& val) {
  return {val};
}

template <typename C, typename T>
struct format_type<C, waybar::lazy::named_arg<T, typename C::char_type>> : std::false_type {};

}

template <typename FuncT, typename Char>
struct formatter<waybar::lazy::named_arg<FuncT, Char>, Char, void> {
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
  auto format(const waybar::lazy::named_arg<FuncT, Char> &lazy, FormatContext &ctx) {
    return format_to(ctx.out(), tm_format, lazy.value());
  }

  std::string tm_format;
};

template <typename FuncT>
struct formatter<waybar::lazy::anon_arg<FuncT>> {
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
  auto format(const waybar::lazy::anon_arg<FuncT> &lazy, FormatContext &ctx) {
    return format_to(ctx.out(), tm_format, lazy.f());
  }

  std::string tm_format;
};
}

