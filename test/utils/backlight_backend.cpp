#include "util/backlight_backend.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <string>
#include <vector>

using waybar::util::BacklightBackend;
using waybar::util::BacklightDevice;

static std::string best_name(const std::vector<BacklightDevice>& devices,
                             const std::string& preferred = "") {
  const auto* best = BacklightBackend::best_device(devices, preferred);
  return best == nullptr ? "<none>" : best->name();
}

TEST_CASE("Backlight auto-selection prefers a screen backlight", "[backlight]") {
  const std::vector<BacklightDevice> devices{
      {"tpacpi::kbd_backlight", 0, 2, true, "leds"},
      {"enp5s0-0::lan", 0, 255, true, "leds"},
      {"amdgpu_bl1", 100, 255, true, "backlight"},
  };
  REQUIRE(best_name(devices) == "amdgpu_bl1");
}

TEST_CASE("Backlight auto-selection never picks an LED", "[backlight]") {
  const std::vector<BacklightDevice> devices{
      {"input3::capslock", 0, 1, true, "leds"},
      {"enp5s0-0::lan", 0, 255, true, "leds"},
      {"white:kbd_backlight", 1, 3, true, "leds"},
  };
  REQUIRE(best_name(devices) == "<none>");
  // A config shared with a laptop names a screen backlight this machine lacks.
  REQUIRE(best_name(devices, "intel_backlight") == "<none>");
}

TEST_CASE("Backlight auto-selection ignores LEDs that are not backlights", "[backlight]") {
  // A desktop without a screen or keyboard backlight (#5462): the module must not
  // pick Caps Lock or a network link LED and show 0%.
  const std::vector<BacklightDevice> devices{
      {"input3::capslock", 0, 1, true, "leds"},
      {"input3::numlock", 1, 1, true, "leds"},
      {"enp5s0-0::lan", 0, 255, true, "leds"},
  };
  REQUIRE(best_name(devices) == "<none>");
  REQUIRE(best_name({}) == "<none>");
}

TEST_CASE("Backlight uses any device named in the config", "[backlight]") {
  const std::vector<BacklightDevice> devices{
      {"amdgpu_bl1", 100, 255, true, "backlight"},
      {"input3::capslock", 0, 1, true, "leds"},
  };
  REQUIRE(best_name(devices, "input3::capslock") == "input3::capslock");
  REQUIRE(best_name(devices, "missing") == "amdgpu_bl1");
}
