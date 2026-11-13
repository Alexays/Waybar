#include "modules/sway/ipc/command.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

using waybar::modules::sway::ipc::quote_arg;
using waybar::modules::sway::ipc::quote_workspace_name;
using waybar::modules::sway::ipc::regex_literal;

TEST_CASE("quote_arg", "[sway]") {
  REQUIRE(quote_arg("1") == R"("1")");
  REQUIRE(quote_arg("") == R"("")");
  REQUIRE(quote_arg("1'") == R"("1'")");
  REQUIRE(quote_arg(R"(10:"Cal")") == R"('10:"Cal"')");
  REQUIRE(quote_arg("a b;c,[d]") == R"("a b;c,[d]")");
  REQUIRE(quote_arg("Ünï ✓") == R"("Ünï ✓")");
  REQUIRE(quote_arg("$mod") == R"("$$mod")");

  REQUIRE_FALSE(quote_arg("a\nb"));
  REQUIRE_FALSE(quote_arg(R"(C:\Users)"));

  // Mixed quotes switch quote characters between adjacent segments.
  REQUIRE(quote_arg(R"(a'b"c)") == R"("a'b"'"c')");
  REQUIRE(quote_arg(R"("a'b)") == R"('"a'"'b")");
  REQUIRE(quote_arg(R"('"')") == R"("'"'"'"'")");

  REQUIRE_FALSE(quote_arg("x\xff"));
  REQUIRE_FALSE(quote_arg("\xc0\xaf"));          // overlong '/'
  REQUIRE_FALSE(quote_arg("\xed\xa0\x80"));      // surrogate U+D800
  REQUIRE_FALSE(quote_arg("\xf4\x90\x80\x80"));  // above U+10FFFF
  REQUIRE(quote_arg("\xf0\x9f\x98\x80"));        // U+1F600
}

TEST_CASE("quote_workspace_name", "[sway]") {
  REQUIRE(quote_workspace_name("1:next") == R"("1:next")");
  REQUIRE_FALSE(quote_workspace_name("next"));
  REQUIRE_FALSE(quote_workspace_name("Output"));
  REQUIRE_FALSE(quote_workspace_name(R"(a\b)"));
}

TEST_CASE("regex_literal", "[sway]") {
  REQUIRE(regex_literal("1") == "1");
  REQUIRE(regex_literal("1'") == R"(1\x{27})");
  REQUIRE(regex_literal(R"(a"b)") == R"(a\x{22}b)");
  REQUIRE(regex_literal("a.b*") == R"(a\x{2e}b\x{2a})");
  REQUIRE(regex_literal("Ünï ✓") == R"(Ünï\x{20}✓)");
}
