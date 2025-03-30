#pragma once

#include <fmt/format.h>
#include <glibmm/ustring.h>

class pow_format {
 public:
  pow_format(long long val, std::string&& unit, bool binary = false)
      : val_(val), unit_(unit), binary_(binary) {};

  long long val_;
  std::string unit_;
  bool binary_;
};

namespace fmt {
template <>
struct formatter<pow_format> {
  char spec = 0;
  int width = 0;

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it == ':') ++it;
    if (it && (*it == '>' || *it == '<' || *it == '=')) {
      spec = *it;
      ++it;
    }
    if (it == end || *it == '}') return it;
    if ('0' <= *it && *it <= '9') {
      // We ignore it for now, but keep it for compatibility with
      // existing configs where the format for pow_format'ed numbers was
      // 'string' and specifications such as {:>9} were valid.
      // The rationale for ignoring it is that the only reason to specify
      // an alignment and a with is to get a fixed width bar, and ">" is
      // sufficient in this implementation.
#if FMT_VERSION < 80000
      width = parse_nonnegative_int(it, end, ctx);
#else
      width = detail::parse_nonnegative_int(it, end, -1);
#endif
    }
    return it;
  }

  template <class FormatContext>
  auto format(const pow_format& s, FormatContext& ctx) const -> decltype(ctx.out()) {
    const char* units[] = {"", "k", "M", "G", "T", "P", nullptr};

    auto base = s.binary_ ? 1024ull : 1000ll;
    auto fraction = (double)s.val_;

    int pow;
    for (pow = 0; units[pow + 1] != nullptr && fraction / base >= 1; ++pow) {
      fraction /= base;
    }

    auto number_width = 5              // coeff in {:.1f} format
                        + s.binary_;   // potential 4th digit before the decimal point
    auto max_width = number_width + 1  // prefix from units array
                     + s.binary_       // for the 'i' in GiB.
                     + s.unit_.length();

    const char* format;
    std::string string;
    switch (spec) {
      case '>':
        return fmt::format_to(ctx.out(), "{:>{}}", fmt::format("{}", s), max_width);
      case '<':
        return fmt::format_to(ctx.out(), "{:<{}}", fmt::format("{}", s), max_width);
      case '=':
        format = "{coefficient:<{number_width}.1f}{padding}{prefix}{unit}";
        break;
      case 0:
      default:
        format = "{coefficient:.1f}{prefix}{unit}";
        break;
    }
    return fmt::format_to(
        ctx.out(), fmt::runtime(format), fmt::arg("coefficient", fraction),
        fmt::arg("number_width", number_width),
        fmt::arg("prefix", std::string() + units[pow] + ((s.binary_ && pow) ? "i" : "")),
        fmt::arg("unit", s.unit_),
        fmt::arg("padding", pow         ? ""
                            : s.binary_ ? "  "
                                        : " "));
  }
};

// Glib ustirng support
template <>
struct formatter<Glib::ustring> : formatter<std::string> {
  template <typename FormatContext>
  auto format(const Glib::ustring& value, FormatContext& ctx) const {
    return formatter<std::string>::format(static_cast<std::string>(value), ctx);
  }
};
}  // namespace fmt
