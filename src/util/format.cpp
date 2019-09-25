#include <sstream>

namespace waybar::util {

std::string pow_format(unsigned long long value, const std::string &unit, bool binary = false) {
  auto base = binary ? 1024ull : 1000ull;
  const char* units[] = { "", "k",  "M",  "G",  "T",  "P",  nullptr};
  auto fraction = (double) value;

  int pow;
  for (pow = 0; units[pow+1] != nullptr && fraction / base >= 2; ++pow) {
    fraction /= base;
  }

  std::ostringstream ss;
  if (pow > 0) {
    auto quotient = (unsigned long long) fraction;
    auto remainder = (unsigned long long) ((fraction - quotient) * 10);
    ss << quotient << "." << remainder << units[pow] << (binary ? "i" : "") << unit;
  } else {
    ss << value << unit;
  }
  return ss.str();
};

}
