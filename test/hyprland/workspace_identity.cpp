#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "modules/hyprland/workspace_identity.hpp"

namespace hyprland = waybar::modules::hyprland;
using hyprland::parseWorkspaceIdentity;
using hyprland::WorkspaceKind;
using hyprland::workspaceSelectorMatchesName;

TEST_CASE("addressable numbered workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["address"] = "3";
  ws["type"] = "numbered";
  ws["name"] = "3";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "3");
  REQUIRE(identity->kind == WorkspaceKind::Numbered);
  REQUIRE(identity->number.has_value());
  REQUIRE(*identity->number == 3);
}

TEST_CASE("addressable special workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["address"] = "special:spotify";
  ws["type"] = "special";
  ws["name"] = "special:spotify";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "special:spotify");
  REQUIRE(identity->kind == WorkspaceKind::Special);
  REQUIRE_FALSE(identity->number.has_value());
}

TEST_CASE("addressable named workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["address"] = "web";
  ws["type"] = "named";
  ws["name"] = "web";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "web");
  REQUIRE(identity->kind == WorkspaceKind::Named);
  REQUIRE_FALSE(identity->number.has_value());
}

TEST_CASE("legacy numeric workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["id"] = 2;
  ws["name"] = "2";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "2");
  REQUIRE(identity->kind == WorkspaceKind::Numbered);
  REQUIRE(*identity->number == 2);
}

TEST_CASE("legacy special workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["id"] = -99;
  ws["name"] = "special";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "-99");
  REQUIRE(identity->kind == WorkspaceKind::Special);
}

TEST_CASE("legacy named workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["id"] = -1377;
  ws["name"] = "web";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->kind == WorkspaceKind::Named);
}

TEST_CASE("unusable workspace payload", "[workspace_identity]") {
  Json::Value ws;
  ws["name"] = "orphan";

  REQUIRE_FALSE(parseWorkspaceIdentity(ws).has_value());
  REQUIRE_FALSE(parseWorkspaceIdentity(Json::Value::nullRef).has_value());
}

TEST_CASE("selector matches a workspace display name", "[workspace_identity]") {
  // A configured "special" must dedupe against the live special workspace,
  // whose address is "-99" on legacy Hyprland and so never matches.
  REQUIRE(workspaceSelectorMatchesName("special", "special"));
  REQUIRE(workspaceSelectorMatchesName("special:spotify", "spotify"));
  REQUIRE(workspaceSelectorMatchesName("name:web", "web"));
  REQUIRE(workspaceSelectorMatchesName("web", "web"));

  REQUIRE_FALSE(workspaceSelectorMatchesName("special:spotify", "special:spotify"));
  REQUIRE_FALSE(workspaceSelectorMatchesName("special:", ""));
  REQUIRE_FALSE(workspaceSelectorMatchesName("", ""));
  REQUIRE_FALSE(workspaceSelectorMatchesName("web", "code"));
}
