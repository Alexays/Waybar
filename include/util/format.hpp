#pragma once

#include <fmt/format.h>
#include <glibmm/ustring.h>

class pow_format {
 public:
  pow_format(long long val, std::string&& unit, bool binary = false, bool skip_decimal = false,
             int min_pow_for_decimal = 0)
      : val_(val),
        unit_(unit),
        binary_(binary),
        skip_decimal_(skip_decimal),
        min_pow_for_decimal_(min_pow_for_decimal) {};

  long long val_;
  std::string unit_;
  bool binary_;
  bool skip_decimal_;
  int min_pow_for_decimal_;
};

namespace fmt {
template <>
struct formatter<pow_format> {
  char spec = 0;           // alignment: '>', '<', '=' (0 = none)
  int width = 0;           // width digits; enforced only when scale_spec != 0
  char scale_spec = 0;     // forced scale: 0 = auto, else one of '#','k','M','G','T','P'
  char unit_pref = 0;      // unit tri-state: 0 = default, 'u' = hide, 'U' = show
  char base_pref = 0;      // base tri-state: 0 = call-site, 'b' = decimal, 'B' = binary
  bool force_int = false;  // 'i': force integer display

  template <typename ParseContext>
  constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it == ':') ++it;
    if (it != end && (*it == '>' || *it == '<' || *it == '=')) {
      spec = *it;
      ++it;
    }
    // Consume scale/flag modifiers and the width in any order, until '}' or end.
    // The width digits (parsed but only enforced when a scale is forced — see
    // format()) may appear anywhere among the modifiers, so both {:=#3} and the
    // more natural {:=3#} are accepted. On an unrecognised char we stop and let
    // fmt raise its usual error.
    while (it != end && *it != '}') {
      char c = *it;
      if (c == '#' || c == 'k' || c == 'M' || c == 'G' || c == 'T' || c == 'P') {
        scale_spec = c;
        ++it;
      } else if (c == 'u' || c == 'U') {
        unit_pref = c;
        ++it;
      } else if (c == 'b' || c == 'B') {
        base_pref = c;
        ++it;
      } else if (c == 'i') {
        force_int = true;
        ++it;
      } else if ('0' <= c && c <= '9') {
        // Width kept for compatibility with existing configs such as {:>9}; only
        // enforced (fixed field + '#' overflow) when a scale is forced.
#if FMT_VERSION < 80000
        width = parse_nonnegative_int(it, end, ctx);
#else
        width = detail::parse_nonnegative_int(it, end, -1);
#endif
      } else {
        break;
      }
    }
    return it;
  }

  template <class FormatContext>
  auto format(const pow_format& s, FormatContext& ctx) const -> decltype(ctx.out()) {
    const char* units[] = {"", "k", "M", "G", "T", "P", nullptr};
    const int max_pow = 5;  // last valid index in units[]

    // Effective base: 'b'/'B' override the call-site binary_.
    bool binary = base_pref == 'B' ? true : base_pref == 'b' ? false : s.binary_;
    auto base = binary ? 1024ull : 1000ll;
    auto div = 1ll;
    auto fraction = (double)s.val_;

    int pow;
    if (scale_spec != 0) {
      // Forced scale: map the char to a fixed index into units[].
      switch (scale_spec) {
        case 'k':
          pow = 1;
          break;
        case 'M':
          pow = 2;
          break;
        case 'G':
          pow = 3;
          break;
        case 'T':
          pow = 4;
          break;
        case 'P':
          pow = 5;
          break;
        default:
          pow = 0;
          break;  // '#' -> base scale
      }
      if (pow > max_pow) pow = max_pow;
      for (int i = 0; i < pow; ++i) div *= base;
      fraction /= div;
    } else {
      for (pow = 0; units[pow + 1] != nullptr && fraction / base >= 1; ++pow) {
        fraction /= base;
        div *= base;
      }
    }

    // Precision: 'i' forces 0; otherwise 1 (or 0 when skip_decimal_ divides
    // evenly). min_pow_for_decimal_ keeps its default-branch-only effect.
    int precision = force_int ? 0 : (s.skip_decimal_ && ((s.val_ % div) == 0)) ? 0 : 1;
    if (!force_int && scale_spec == 0 && pow < s.min_pow_for_decimal_) precision = 0;

    // Unit visibility: default on for auto scale, off for a forced scale; 'u'/'U'
    // override. The binary 'i' is part of the scale prefix, so the unit is just
    // unit_.
    bool hide_unit = unit_pref == 'u' || (unit_pref == 0 && scale_spec != 0);

    // Scale prefix (letter + binary 'i'), suppressed entirely when a scale is
    // forced.
    std::string prefix =
        scale_spec != 0 ? "" : std::string(units[pow]) + ((binary && pow) ? "i" : "");
    std::string unit = hide_unit ? "" : s.unit_;

    auto number_width = 3 + precision       // coeff in {:.{precision}f} format
                        + (precision != 0)  // float dot
                        + binary;           // potential digit before the decimal point
    // In auto mode the prefix column is always reserved (letter + optional 'i'),
    // matching the historical fixed max_width even at base scale (the '=' padding
    // fills the gap). A forced scale drops the prefix column entirely.
    auto prefix_col = scale_spec != 0 ? 0 : 1 + binary;
    auto max_width = number_width + prefix_col + unit.length();

    // The numeric coefficient string. When a scale is forced with a width and the
    // number does not fit, it overflows to '#' (spreadsheet-style).
    bool fixed_num = scale_spec != 0 && width > 0;
    std::string number = fmt::format("{:.{}f}", fraction, precision);
    if (fixed_num && (int)number.length() > width) number = std::string(width, '#');

    // Base-scale compensation for the '=' column-align: only in auto mode, where
    // the absent prefix (and binary 'i') would otherwise shift the unit column.
    const char* padding = (scale_spec == 0 && pow == 0) ? (binary ? "  " : " ") : "";

    switch (spec) {
      case '=':
        // Column-align: left-justify the coefficient within its column, then pad
        // so the prefix/unit line up across values of different magnitude.
        return fmt::format_to(ctx.out(), "{:<{}}{}{}{}", number, fixed_num ? width : number_width,
                              padding, prefix, unit);
      case '>':
      case '<':
      case 0:
      default: {
        // Right-justify the numeric field to the fixed width when forced.
        std::string body =
            (fixed_num ? fmt::format("{:>{}}", number, width) : number) + prefix + unit;
        if (spec == '>') return fmt::format_to(ctx.out(), "{:>{}}", body, max_width);
        if (spec == '<') return fmt::format_to(ctx.out(), "{:<{}}", body, max_width);
        return fmt::format_to(ctx.out(), "{}", body);
      }
    }
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
