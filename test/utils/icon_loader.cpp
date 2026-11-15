#include "util/icon_loader.hpp"

#include <giomm/init.h>
#include <glibmm/miscutils.h>
#include <gtkmm/icontheme.h>

#include <cstdlib>
#include <vector>

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

namespace {

// WaybarTestChild ships waybar-test-child-only and inherits WaybarTestParent,
// which ships waybar-test-parent-only. Neither theme ships the other's icon.
Glib::RefPtr<Gtk::IconTheme> child_theme() {
  auto theme = Gtk::IconTheme::create();
  theme->set_search_path({fixtures + "/icons"});
  theme->set_custom_theme("WaybarTestChild");
  return theme;
}

}  // namespace

TEST_CASE("an inherited icon does not count as the theme's own", "[util][icon_loader]") {
  auto theme = child_theme();

  SECTION("a plain lookup resolves through Inherits=") {
    // Not the behavior under test, but the reason the check below has to exist:
    // the child ships nothing by this name, yet the lookup still succeeds.
    REQUIRE(theme->lookup_icon("waybar-test-parent-only", 48));
  }

  SECTION("only an icon the theme itself ships is native to it") {
    REQUIRE(IconLoader::icon_is_native_to_theme(theme, "WaybarTestChild", "waybar-test-child-only",
                                                48));
    REQUIRE_FALSE(IconLoader::icon_is_native_to_theme(theme, "WaybarTestChild",
                                                      "waybar-test-parent-only", 48));
  }

  SECTION("an icon no theme in the chain ships is not native either") {
    REQUIRE_FALSE(
        IconLoader::icon_is_native_to_theme(theme, "WaybarTestChild", "waybar-test-absent", 48));
  }

  SECTION("empty names are never native") {
    REQUIRE_FALSE(IconLoader::icon_is_native_to_theme(theme, "", "waybar-test-child-only", 48));
    REQUIRE_FALSE(IconLoader::icon_is_native_to_theme(theme, "WaybarTestChild", "", 48));
  }
}
