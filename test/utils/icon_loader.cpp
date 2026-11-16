#include "util/icon_loader.hpp"

#include <giomm/init.h>
#include <glib.h>
#include <glibmm/miscutils.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>

#include "util/steam_icon.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

static const std::string fixtures = Glib::get_current_dir() + "/test/utils/fixtures";

// IconLoader and GIO read the search path once, so every case pins the same one first.
static void use_fixture_data_dirs() {
  setenv("XDG_DATA_DIRS", fixtures.c_str(), 1);
  setenv("XDG_DATA_HOME", fixtures.c_str(), 1);
  Gio::init();
}

TEST_CASE("IconLoader ignores app ids that are not UTF-8", "[util][icon_loader]") {
  use_fixture_data_dirs();
  for (const char* app_id : {"\xc1\xa1", "\xe0\x81\xa1", "\xc0\xb1",
                             "a\xc1\xa1"
                             "b",
                             "org.example.application\xf0"}) {
    REQUIRE_FALSE(IconLoader::get_app_info_from_app_id_list(app_id));
  }
}

TEST_CASE("IconLoader resolves desktop entries by name only", "[util][icon_loader]") {
  use_fixture_data_dirs();

  SECTION("a bare name is looked up below the data dirs") {
    auto info = IconLoader::get_app_info_from_app_id_list("waybar-test");
    REQUIRE(info);
    REQUIRE(info->get_name() == "Waybar Test");
  }

  SECTION("an app id that is a path is never opened") {
    REQUIRE_FALSE(
        IconLoader::get_app_info_from_app_id_list(fixtures + "/applications/waybar-test.desktop"));
    REQUIRE_FALSE(
        IconLoader::get_app_info_from_app_id_list("../fixtures/applications/waybar-test"));
  }
}

TEST_CASE("Steam icon helper extracts and validates Steam app ids", "[util][icon_loader]") {
  using namespace waybar::util::steam;

  REQUIRE(extract_app_id("steam_app_359320") == "359320");
  REQUIRE(extract_app_id("Steam_App_359320") == "359320");
  REQUIRE(extract_app_id("steam_app_0") == "0");
  REQUIRE(extract_app_id("steam_app_00") == "00");

  REQUIRE_FALSE(extract_app_id(""));
  REQUIRE_FALSE(extract_app_id("steam_app_"));
  REQUIRE_FALSE(extract_app_id("steam_app_abc"));
  REQUIRE_FALSE(extract_app_id("steam_app_-1"));
  REQUIRE_FALSE(extract_app_id("steam_app_359320/foo"));
  REQUIRE_FALSE(extract_app_id("steam_app_../359320"));
  REQUIRE_FALSE(extract_app_id("steam_app_default"));
  REQUIRE_FALSE(extract_app_id("steam_app_123456789012345678901"));
  REQUIRE_FALSE(extract_app_id("firefox"));

  REQUIRE(is_default_app_id("steam_app_default"));
  REQUIRE(is_default_app_id("Steam_App_Default"));
  REQUIRE(is_default_app_id("STEAM_APP_DEFAULT"));
  REQUIRE_FALSE(is_default_app_id("steam_app_0"));
  REQUIRE_FALSE(is_default_app_id("steam_app_359320"));
  REQUIRE_FALSE(is_default_app_id("steam_app_default_2"));
  REQUIRE_FALSE(is_default_app_id(""));

  REQUIRE(is_fallback_app_id("steam_app_359320"));
  REQUIRE(is_fallback_app_id("steam_app_0"));
  REQUIRE(is_fallback_app_id("steam_app_default"));
  REQUIRE(is_fallback_app_id("Steam_App_Default"));
  REQUIRE_FALSE(is_fallback_app_id("firefox"));
  REQUIRE_FALSE(is_fallback_app_id("unknown"));
  REQUIRE_FALSE(is_fallback_app_id("steam_app_abc"));

  REQUIRE(has_app_id("steam_app_359320"));
  REQUIRE(has_app_id("Steam_App_359320"));
  REQUIRE(has_app_id("other steam_app_359320"));
  REQUIRE_FALSE(has_app_id("steam_app_0"));
  REQUIRE_FALSE(has_app_id("steam_app_00"));
  REQUIRE_FALSE(has_app_id("steam_app_default"));
  REQUIRE_FALSE(has_app_id("Steam_App_Default"));
  REQUIRE_FALSE(has_app_id("unknown"));

  REQUIRE(should_skip_title_fallback("steam_app_359320"));
  REQUIRE(should_skip_title_fallback("other steam_app_359320"));
  REQUIRE(should_skip_title_fallback("steam_app_default"));
  REQUIRE(should_skip_title_fallback("Steam_App_Default"));
  REQUIRE(should_skip_title_fallback("other Steam_App_Default"));
  REQUIRE_FALSE(should_skip_title_fallback("steam_app_0"));
  REQUIRE_FALSE(should_skip_title_fallback("steam_app_00"));
  REQUIRE_FALSE(should_skip_title_fallback("firefox"));
  REQUIRE_FALSE(should_skip_title_fallback("unknown"));
}

TEST_CASE("IconLoader resolves Steam desktop entries via StartupWMClass, Exec, and Icon",
          "[util][icon_loader]") {
  use_fixture_data_dirs();

  SECTION("matches Exec containing steam://rungameid/<appid>") {
    auto info = IconLoader::get_app_info_from_app_id_list("steam_app_999991");
    REQUIRE(info);
    REQUIRE(info->get_name() == "Roadwarden");
  }

  SECTION("does not match partial appid prefixes") {
    REQUIRE_FALSE(IconLoader::get_app_info_from_app_id_list("steam_app_99999"));
  }

  SECTION("matches StartupWMClass=steam_app_<appid>") {
    auto info = IconLoader::get_app_info_from_app_id_list("steam_app_999992");
    REQUIRE(info);
    REQUIRE(info->get_name() == "Proton WMClass Game");
  }

  SECTION("skips keyword search for steam_app_default and steam_app_0") {
    REQUIRE_FALSE(IconLoader::get_app_info_from_app_id_list("steam_app_default"));
    REQUIRE_FALSE(IconLoader::get_app_info_from_app_id_list("Steam_App_Default"));
    REQUIRE_FALSE(IconLoader::get_app_info_from_app_id_list("steam_app_0"));
  }

  SECTION("returns null when no Steam desktop entry exists") {
    REQUIRE_FALSE(IconLoader::get_app_info_from_app_id_list("steam_app_359320"));
  }
}

TEST_CASE("IconLoader title lookup avoids keyword collisions and respects Steam fallback rules",
          "[util][icon_loader]") {
  use_fixture_data_dirs();

  REQUIRE_FALSE(IconLoader::get_app_info_from_title("elite launcher"));
  REQUIRE_FALSE(IconLoader::get_app_info_from_title("Elite - Dangerous (CLIENT)"));
  REQUIRE_FALSE(IconLoader::get_app_info_from_title("Elite Dangerous"));
  REQUIRE_FALSE(IconLoader::get_app_info_from_title("   "));
  REQUIRE_FALSE(IconLoader::get_app_info_from_title("\xc1\xa1"));

  auto companion = IconLoader::get_app_info_from_title("Planisphere");
  REQUIRE(companion);
  REQUIRE(companion->get_name() == "Planisphere");

  auto companion_lower = IconLoader::get_app_info_from_title("planisphere");
  REQUIRE(companion_lower);
  REQUIRE(companion_lower->get_name() == "Planisphere");

  auto test_app = IconLoader::get_app_info_from_title("Waybar Test");
  REQUIRE(test_app);
  REQUIRE(test_app->get_name() == "Waybar Test");

  // Non-Steam shortcuts (steam_app_0) still allow exact title fallback, unlike steam_app_<id> and
  // steam_app_default.
  REQUIRE_FALSE(waybar::util::steam::should_skip_title_fallback("steam_app_0"));
  REQUIRE(waybar::util::steam::should_skip_title_fallback("steam_app_359320"));
  REQUIRE(waybar::util::steam::should_skip_title_fallback("Steam_App_Default"));
}

TEST_CASE("Steam icon helper locates Steam librarycache icons", "[util][icon_loader]") {
  use_fixture_data_dirs();
  using waybar::util::steam::find_library_icon;

  struct ScopedTempEnv {
    std::filesystem::path root;
    std::optional<std::string> old_xdg;
    std::optional<std::string> old_home;

    ScopedTempEnv() {
      gchar* raw = g_dir_make_tmp("waybar-steam-test-XXXXXX", nullptr);
      REQUIRE(raw != nullptr);
      root = raw;
      g_free(raw);
      if (const char* v = std::getenv("XDG_DATA_HOME")) old_xdg = v;
      if (const char* v = std::getenv("HOME")) old_home = v;
      setenv("XDG_DATA_HOME", root.c_str(), 1);
      setenv("HOME", root.c_str(), 1);
    }

    ~ScopedTempEnv() {
      if (old_xdg)
        setenv("XDG_DATA_HOME", old_xdg->c_str(), 1);
      else
        unsetenv("XDG_DATA_HOME");
      if (old_home)
        setenv("HOME", old_home->c_str(), 1);
      else
        unsetenv("HOME");
      std::error_code ec;
      std::filesystem::remove_all(root, ec);
    }
  } env;

  const auto cache_dir = env.root / "Steam/appcache/librarycache";

  SECTION("ignores banners and 0-byte files, picking smallest non-empty 40-hex") {
    const auto app_dir = cache_dir / "359320";
    std::filesystem::create_directories(app_dir);

    {
      std::ofstream(app_dir / "header.jpg") << "banner";
    }
    {
      std::ofstream(app_dir / "library_600x900.jpg") << "poster";
    }
    {
      std::ofstream(app_dir / "library_hero.jpg") << "hero";
    }
    {
      std::ofstream(app_dir / "logo.png") << "logo";
    }
    {
      std::ofstream(app_dir / "0000000000000000000000000000000000000000.jpg");
    }
    {
      std::ofstream(app_dir / "icon.png");
    }
    REQUIRE(find_library_icon("359320").empty());

    const auto large_hex = app_dir / "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.jpg";
    {
      std::ofstream(large_hex) << std::string(200, 'a');
    }
    const auto small_hex = app_dir / "670f2f289a180f7ac291585df847009640ebf1c5.jpg";
    {
      std::ofstream(small_hex) << std::string(16, 'i');
    }
    REQUIRE(find_library_icon("359320") == small_hex.string());

    // Deterministic path tie-break when two 40-hex files have the exact same non-zero size
    const auto tie_hex = app_dir / "1111111111111111111111111111111111111111.jpg";
    {
      std::ofstream(tie_hex) << std::string(16, 't');
    }
    REQUIRE(find_library_icon("359320") == tie_hex.string());
  }

  SECTION("scans depth-1 subdirectories and prefers icon.* over 40-hex") {
    const auto app_dir = cache_dir / "359322";
    const auto sub_dir = app_dir / "hashed_assets";
    std::filesystem::create_directories(sub_dir);

    const auto app_hex = app_dir / "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.jpg";
    {
      std::ofstream(app_hex) << std::string(200, 'a');
    }
    const auto sub_hex = sub_dir / "0123456789abcdef0123456789abcdef01234567.jpg";
    {
      std::ofstream(sub_hex) << std::string(12, 's');
    }
    REQUIRE(find_library_icon("359322") == sub_hex.string());

    const auto named_icon = sub_dir / "icon.png";
    {
      std::ofstream(named_icon) << std::string(512, 'n');
    }
    REQUIRE(find_library_icon("359322") == named_icon.string());

    const auto root_named = app_dir / "icon.ico";
    {
      std::ofstream(root_named) << std::string(1024, 'r');
    }
    REQUIRE(find_library_icon("359322") == root_named.string());
  }

  SECTION("skips 0-byte legacy icon and resolves non-empty legacy icon") {
    std::filesystem::create_directories(cache_dir);
    {
      std::ofstream(cache_dir / "359321_icon.png");
    }
    REQUIRE(find_library_icon("359321").empty());

    const auto legacy_icon = cache_dir / "359321_icon.jpg";
    {
      std::ofstream(legacy_icon) << "legacy";
    }
    REQUIRE(find_library_icon("359321") == legacy_icon.string());
  }

  REQUIRE(find_library_icon("0").empty());
  REQUIRE(find_library_icon("default").empty());
  REQUIRE(find_library_icon("../359320").empty());
}
