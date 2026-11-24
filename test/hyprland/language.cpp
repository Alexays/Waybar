#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "modules/hyprland/language_format.hpp"

using waybar::modules::hyprland::formatLanguage;

TEST_CASE("hyprland/language format-<lang> overrides named fallback format (#5295)",
          "[hyprland][language]") {
  Json::Value cfg(Json::objectValue);
  cfg["format"] = "{short}";
  cfg["format-fa"] = "IR";
  cfg["format-en"] = "US";

  REQUIRE(formatLanguage("English (US)", "us", "", "en", cfg, "{short}", "format") == "US");
  REQUIRE(formatLanguage("Persian", "ir", "", "fa", cfg, "{short}", "format") == "IR");
  // Unmatched languages fall back to base format.
  REQUIRE(formatLanguage("German", "de", "", "de", cfg, "{short}", "format") == "de");
}

TEST_CASE("hyprland/language supports man-page examples and wrapper formats",
          "[hyprland][language]") {
  SECTION("man-page example 1: format without {}") {
    Json::Value cfg(Json::objectValue);
    cfg["format"] = "Lang: {long}";
    cfg["format-en"] = "AMERICA, HELL YEAH!";

    REQUIRE(formatLanguage("English (US)", "us", "", "en", cfg, "Lang: {long}", "format") ==
            "AMERICA, HELL YEAH!");
    REQUIRE(formatLanguage("German", "de", "", "de", cfg, "Lang: {long}", "format") ==
            "Lang: German");
  }

  SECTION("wrapper format with {} and named placeholders inside format-<lang>") {
    Json::Value cfg(Json::objectValue);
    cfg["format"] = "Lang: {}";
    cfg["format-en"] = "EN ({short})";

    REQUIRE(formatLanguage("English (US)", "us", "", "en", cfg, "Lang: {}", "format") ==
            "Lang: EN (us)");
  }

  SECTION("escaped braces in base format are not treated as {}") {
    Json::Value cfg(Json::objectValue);
    cfg["format-en"] = "US";

    REQUIRE(formatLanguage("English (US)", "us", "", "en", cfg, "{{}} {short}", "format") == "US");
  }

  SECTION("variant precedence and empty variant handling") {
    Json::Value cfg(Json::objectValue);
    cfg["format-en"] = "EN";
    cfg["format-en-dvorak"] = "EN-DV";
    cfg["format-en-"] = "BOGUS";

    REQUIRE(formatLanguage("English (Dvorak)", "us", "dvorak", "en", cfg, "{}", "format") ==
            "EN-DV");
    REQUIRE(formatLanguage("English (US)", "us", "", "en", cfg, "{}", "format") == "EN");
  }

  SECTION("tooltip-format default {long} and literal non-placeholder braces") {
    Json::Value cfg(Json::objectValue);
    cfg["tooltip-format-en"] = "Custom English";
    cfg["tooltip-format-es"] = "{Español}";

    REQUIRE(formatLanguage("English (US)", "us", "", "en", cfg, "{long}", "tooltip-format") ==
            "Custom English");
    REQUIRE(formatLanguage("Spanish", "es", "", "es", cfg, "{}", "tooltip-format") == "{Español}");
  }
}
