#include "util/icon_loader.hpp"

#include <giomm/init.h>
#include <glibmm/miscutils.h>

#include <cstdlib>

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
