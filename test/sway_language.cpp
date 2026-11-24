#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "modules/sway/language_input.hpp"

namespace sway = waybar::modules::sway;

namespace {
Json::Value makeKeyboard(const std::string& identifier, const std::vector<std::string>& layouts,
                         const std::string& active) {
  Json::Value dev(Json::objectValue);
  dev["identifier"] = identifier;
  dev["type"] = "keyboard";
  Json::Value arr(Json::arrayValue);
  for (const auto& l : layouts) {
    arr.append(l);
  }
  dev["xkb_layout_names"] = arr;
  dev["xkb_active_layout_name"] = active;
  return dev;
}

Json::Value makeEvent(const std::string& change, const Json::Value& input) {
  Json::Value ev(Json::objectValue);
  ev["change"] = change;
  ev["input"] = input;
  return ev;
}
}  // namespace

TEST_CASE("sway/language virtual keyboard detection", "[sway][language]") {
  REQUIRE(sway::is_virtual_keyboard(
      makeKeyboard("0:0:wlr_virtual_keyboard_v1", {"English (US)"}, "English (US)")));
  REQUIRE(sway::is_virtual_keyboard(
      makeKeyboard("0:0:Virtual_Keyboard", {"English (US)"}, "English (US)")));
  // Non-virtual 0:0 hardware devices (e.g. gpio-keys) and USB keyboards are physical.
  REQUIRE_FALSE(
      sway::is_virtual_keyboard(makeKeyboard("0:0:gpio-keys", {"English (US)"}, "English (US)")));
  REQUIRE_FALSE(sway::is_virtual_keyboard(
      makeKeyboard("1575:1:QEMU_QEMU_USB_Keyboard", {"English (US)"}, "English (US)")));
}

TEST_CASE("sway/language excludes virtual keyboards when physical keyboard exists",
          "[sway][language]") {
  Json::Value inputs(Json::arrayValue);
  inputs.append(makeKeyboard("1:1:AT_Translated_Set_2_keyboard", {"Portuguese (Brazil)"},
                             "Portuguese (Brazil)"));
  inputs.append(makeKeyboard("0:0:wlr_virtual_keyboard_v1", {"English (US)"}, "English (US)"));

  const auto snap = sway::collect_keyboard_layouts(inputs);
  REQUIRE(snap.found_keyboard);
  REQUIRE(snap.has_physical_keyboard);
  REQUIRE(snap.used_layouts == std::vector<std::string>{"Portuguese (Brazil)"});
  REQUIRE(snap.active_layout == "Portuguese (Brazil)");

  // Virtual keyboard events are ignored while a physical keyboard is present.
  const auto virtEv = makeEvent(
      "xkb_keymap", makeKeyboard("0:0:wlr_virtual_keyboard_v1", {"English (US)"}, "English (US)"));
  const auto virtAction = sway::classify_input_event(virtEv, snap.has_physical_keyboard);
  REQUIRE_FALSE(virtAction.refresh_inputs);
  REQUIRE_FALSE(virtAction.update_layout);
}

TEST_CASE("sway/language falls back to virtual keyboards when no physical keyboard exists",
          "[sway][language]") {
  Json::Value inputs(Json::arrayValue);
  inputs.append(
      makeKeyboard("0:0:wlr_virtual_keyboard_v1", {"English (US)", "French"}, "English (US)"));

  const auto snap = sway::collect_keyboard_layouts(inputs);
  REQUIRE(snap.found_keyboard);
  REQUIRE_FALSE(snap.has_physical_keyboard);
  REQUIRE(snap.used_layouts == std::vector<std::string>{"English (US)", "French"});
  REQUIRE(snap.active_layout == "English (US)");

  const auto switchEv = makeEvent("xkb_layout", makeKeyboard("0:0:wlr_virtual_keyboard_v1",
                                                             {"English (US)", "French"}, "French"));
  const auto switchAction = sway::classify_input_event(switchEv, snap.has_physical_keyboard);
  REQUIRE_FALSE(switchAction.refresh_inputs);
  REQUIRE(switchAction.update_layout);
  REQUIRE(switchAction.active_layout == "French");
}

TEST_CASE("sway/language refreshes on keyboard removal without adopting removed layout",
          "[sway][language]") {
  const auto removedUsb =
      makeKeyboard("1575:1:QEMU_QEMU_USB_Keyboard", {"English (US)"}, "English (US)");
  const auto action = sway::classify_input_event(makeEvent("removed", removedUsb), true);
  REQUIRE(action.refresh_inputs);
  REQUIRE_FALSE(action.update_layout);
}
