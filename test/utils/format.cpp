#include "util/format.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <fmt/format.h>

// Helpers to build the two representative sample values used throughout the
// pow_format modifier tests.
static std::string fmtA(const char* spec) {
  // 1536 bytes, binary (base 1024) -> auto "1.5kiB"
  return fmt::format(fmt::runtime(std::string("{:") + spec + "}"), pow_format(1536, "B", true));
}
static std::string fmtB(const char* spec) {
  // 1500000 b/s, decimal (base 1000) -> auto "1.5Mb/s"
  return fmt::format(fmt::runtime(std::string("{:") + spec + "}"), pow_format(1500000, "b/s"));
}

TEST_CASE("pow_format default/auto rendering", "[format][pow_format]") {
  REQUIRE(fmt::format("{}", pow_format(1536, "B", true)) == "1.5kiB");
  REQUIRE(fmt::format("{}", pow_format(1500000, "b/s")) == "1.5Mb/s");
  // Sub-scale values and unit-only rendering.
  REQUIRE(fmt::format("{}", pow_format(500, "B")) == "500.0B");
}

TEST_CASE("pow_format hide/show unit (u/U)", "[format][pow_format]") {
  REQUIRE(fmtA("u") == "1.5ki");   // unit hidden, scale prefix (ki) kept
  REQUIRE(fmtA("U") == "1.5kiB");  // explicit show = auto default
  REQUIRE(fmtB("u") == "1.5M");
  REQUIRE(fmtB("U") == "1.5Mb/s");
}

TEST_CASE("pow_format force integer (i)", "[format][pow_format]") {
  REQUIRE(fmtA("i") == "2kiB");  // 1.5 -> 2
  REQUIRE(fmtA("iu") == "2ki");
  REQUIRE(fmtB("i") == "2Mb/s");
}

TEST_CASE("pow_format forced scale hides prefix and unit", "[format][pow_format]") {
  // Base scale (#): no prefix; unit off by default, back on with U.
  REQUIRE(fmtA("#") == "1536.0");
  REQUIRE(fmtA("#U") == "1536.0B");
  REQUIRE(fmtA("#i") == "1536");
  REQUIRE(fmtB("#") == "1500000.0");
  REQUIRE(fmtB("#U") == "1500000.0b/s");

  // Force kilo: whole prefix (ki) suppressed, unit off by default.
  REQUIRE(fmtA("k") == "1.5");
  REQUIRE(fmtA("kU") == "1.5B");  // unit shown, no 'i' (it lives on the prefix)
  REQUIRE(fmtA("ki") == "2");
  REQUIRE(fmtB("k") == "1500.0");
  REQUIRE(fmtB("kU") == "1500.0b/s");
  REQUIRE(fmtB("ki") == "1500");

  // Force a scale far above the value's magnitude -> underflow to 0, no bump.
  REQUIRE(fmtA("M") == "0.0");
  REQUIRE(fmtA("MU") == "0.0B");
  REQUIRE(fmtB("G") == "0.0");
}

TEST_CASE("pow_format force base (b/B)", "[format][pow_format]") {
  // Force decimal on a binary call-site value.
  REQUIRE(fmtA("b") == "1.5kB");  // base 1000, prefix 'k', no 'i'
  REQUIRE(fmtA("bu") == "1.5k");
  REQUIRE(fmtA("b#") == "1536.0");

  // Force binary on a decimal call-site value.
  REQUIRE(fmtB("B") == "1.4Mib/s");  // base 1024, prefix 'Mi'
  REQUIRE(fmtB("Bu") == "1.4Mi");
  REQUIRE(fmtB("Bk") == "1464.8");
  REQUIRE(fmtB("BkU") == "1464.8b/s");
}

TEST_CASE("pow_format fixed width with # overflow", "[format][pow_format]") {
  // Base scale, numeric field width 3: 1536.0 (6 chars) overflows -> ###.
  REQUIRE(fmtA("=3#") == "###");
  REQUIRE(fmtA("=6#") == "1536.0");
  REQUIRE(fmtA("=3#U") == "###B");
  REQUIRE(fmtA("=6#U") == "1536.0B");
  REQUIRE(fmtB("=4k#") == "####");
  // Width digit position is flexible: {:=3#} and {:=#3} are equivalent.
  REQUIRE(fmtA("=#3") == "###");
  // Force kilo, width 6, coefficient "1500" fits, shown with unit; the '='
  // column left-justifies the coefficient in its field.
  REQUIRE(fmtB("=6kiU") == "1500  b/s");
}

TEST_CASE("pow_format modifiers compose with alignment", "[format][pow_format]") {
  REQUIRE(fmtA(">u") == "   1.5ki");  // right-align, unit hidden (prefix kept)
  REQUIRE(fmtB(">ki") == "1500");     // force kilo+int (unit off), no scale width
  REQUIRE(fmtB("Bu") == "1.4Mi");     // force binary, unit hidden
}

TEST_CASE("pow_format backward compatible alignment", "[format][pow_format]") {
  // These specs predate the new modifiers and must render as before.
  REQUIRE(fmt::format("{:>}", pow_format(1536, "B", true)) == "   1.5kiB");
  REQUIRE(fmt::format("{:<}", pow_format(1536, "B", true)) == "1.5kiB   ");
  REQUIRE(fmt::format("{:=}", pow_format(1536, "B", true)) == "1.5   kiB");
  // Width without a forced scale is still ignored (only used for compat).
  REQUIRE(fmt::format("{:>9}", pow_format(1536, "B", true)) == "   1.5kiB");
  REQUIRE(fmt::format("{}", pow_format(1500000, "b/s")) == "1.5Mb/s");
}
