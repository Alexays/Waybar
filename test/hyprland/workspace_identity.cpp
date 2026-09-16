#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "modules/hyprland/workspace_identity.hpp"

namespace hyprland = waybar::modules::hyprland;
using hyprland::isGenericSpecialName;
using hyprland::parseWorkspaceIdentity;
using hyprland::parseWorkspaceSelector;
using hyprland::workspaceDisplayName;
using hyprland::WorkspaceKind;
using hyprland::workspaceMatchesIdentifier;
using hyprland::workspaceRawName;
using hyprland::WorkspaceSelector;

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

TEST_CASE("legacy workspace merely named like the special namespace", "[workspace_identity]") {
  // `specialfoo` is not under `special:`, so it is an ordinary named workspace.
  Json::Value ws;
  ws["id"] = -1377;
  ws["name"] = "specialfoo";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->kind == WorkspaceKind::Named);
}

TEST_CASE("display name strips exactly one special prefix", "[workspace_identity]") {
  REQUIRE(workspaceDisplayName("special:spotify", WorkspaceKind::Special) == "spotify");

  // A special workspace the user named `special:123` is reported by Hyprland as
  // `special:special:123`, so only the outer prefix belongs to the namespace.
  REQUIRE(workspaceDisplayName("special:special:123", WorkspaceKind::Special) == "special:123");

  // The generic special workspace carries no name after the namespace.
  REQUIRE(workspaceDisplayName("special", WorkspaceKind::Special) == "special");

  // A special workspace the user named `special` is reported as
  // `special:special` and displays as `special` -- same text, different
  // workspace. isGenericSpecialName() is what keeps them apart.
  REQUIRE(workspaceDisplayName("special:special", WorkspaceKind::Special) == "special");
  REQUIRE(isGenericSpecialName("special", WorkspaceKind::Special));
  REQUIRE_FALSE(isGenericSpecialName("special:special", WorkspaceKind::Special));
  REQUIRE_FALSE(isGenericSpecialName("special", WorkspaceKind::Named));
}

TEST_CASE("display name never strips the name: selector prefix", "[workspace_identity]") {
  // `name:` is selector syntax; Hyprland never prepends it to a reported name.
  // A workspace actually called `name:foo` must keep that name.
  REQUIRE(workspaceDisplayName("name:foo", WorkspaceKind::Named) == "name:foo");
  REQUIRE(workspaceDisplayName("name:foo", WorkspaceKind::Numbered) == "name:foo");

  // A special workspace the user named `name:foo` is reported as
  // `special:name:foo`: the namespace goes, the rest stays.
  REQUIRE(workspaceDisplayName("special:name:foo", WorkspaceKind::Special) == "name:foo");
}

TEST_CASE("selectors split into kind and name", "[workspace_identity]") {
  auto selector = parseWorkspaceSelector("7");
  REQUIRE(selector.kind == WorkspaceKind::Numbered);
  REQUIRE(selector.name == "7");

  selector = parseWorkspaceSelector("web");
  REQUIRE(selector.kind == WorkspaceKind::Named);
  REQUIRE(selector.name == "web");

  selector = parseWorkspaceSelector("name:web");
  REQUIRE(selector.kind == WorkspaceKind::Named);
  REQUIRE(selector.name == "web");

  selector = parseWorkspaceSelector("special");
  REQUIRE(selector.kind == WorkspaceKind::Special);
  REQUIRE(selector.name == "special");

  selector = parseWorkspaceSelector("special:spotify");
  REQUIRE(selector.kind == WorkspaceKind::Special);
  REQUIRE(selector.name == "spotify");

  // A named workspace `foo` and a special one `special:foo` display the same
  // name; only the kind separates them.
  REQUIRE(parseWorkspaceSelector("foo").kind == WorkspaceKind::Named);
  REQUIRE(parseWorkspaceSelector("special:foo").kind == WorkspaceKind::Special);
  REQUIRE(parseWorkspaceSelector("foo").name == parseWorkspaceSelector("special:foo").name);

  // `specialfoo` is not under the special namespace.
  REQUIRE(parseWorkspaceSelector("specialfoo").kind == WorkspaceKind::Named);
}

TEST_CASE("a bare selector prefix selects nothing", "[workspace_identity]") {
  // Stripping these would leave an empty name that matches every workspace of
  // the kind, so they are left intact and simply match nothing.
  REQUIRE(parseWorkspaceSelector("special:").name == "special:");
  REQUIRE(parseWorkspaceSelector("name:").name == "name:");
  REQUIRE(parseWorkspaceSelector("").name.empty());
}

TEST_CASE("raw name round-trips through the display name", "[workspace_identity]") {
  for (const auto* selectorText : {"7", "web", "name:web", "special", "special:spotify",
                                   "special:special", "special:name:foo", "specialfoo"}) {
    const auto selector = parseWorkspaceSelector(selectorText);
    const auto rawName = workspaceRawName(selector);
    INFO("selector: " << selectorText << " raw: " << rawName);
    REQUIRE(workspaceDisplayName(rawName, selector.kind) == selector.name);
  }
}

TEST_CASE("raw name restores the namespace Hyprland would report", "[workspace_identity]") {
  REQUIRE(workspaceRawName(parseWorkspaceSelector("special:spotify")) == "special:spotify");
  REQUIRE(workspaceRawName(parseWorkspaceSelector("special")) == "special");
  REQUIRE(workspaceRawName(parseWorkspaceSelector("name:web")) == "web");
  REQUIRE(workspaceRawName(parseWorkspaceSelector("7")) == "7");

  // A configured `special:special` is the user's own workspace named `special`,
  // not the generic one, so its raw name keeps the namespace.
  REQUIRE(workspaceRawName(parseWorkspaceSelector("special:special")) == "special:special");
}

TEST_CASE("event identifiers resolve to the workspace they name", "[workspace_identity]") {
  // Hyprland announces a workspace by the identifier it was created with, which
  // is not always the address `workspaces` reports. Observed on 0.56.0:
  //   createworkspacev2>>name:web,web              -> address "web"
  //   createworkspacev2>>special:newspecial,...    -> address "special:newspecial"
  // so a named workspace is announced by selector and reported by address.
  Json::Value named;
  named["address"] = "web";
  named["type"] = "named";
  named["name"] = "web";
  const auto namedIdentity = parseWorkspaceIdentity(named);
  REQUIRE(namedIdentity.has_value());

  REQUIRE(workspaceMatchesIdentifier("name:web", *namedIdentity, "web"));
  REQUIRE(workspaceMatchesIdentifier("web", *namedIdentity, "web"));
  REQUIRE_FALSE(workspaceMatchesIdentifier("name:other", *namedIdentity, "web"));
  // A special workspace displaying the same name is a different workspace.
  REQUIRE_FALSE(workspaceMatchesIdentifier("special:web", *namedIdentity, "web"));

  Json::Value special;
  special["address"] = "special:newspecial";
  special["type"] = "special";
  special["name"] = "special:newspecial";
  const auto specialIdentity = parseWorkspaceIdentity(special);
  REQUIRE(specialIdentity.has_value());

  REQUIRE(workspaceMatchesIdentifier("special:newspecial", *specialIdentity, "special:newspecial"));
  REQUIRE_FALSE(workspaceMatchesIdentifier("newspecial", *specialIdentity, "special:newspecial"));

  Json::Value numbered;
  numbered["address"] = "3";
  numbered["type"] = "numbered";
  numbered["name"] = "3";
  const auto numberedIdentity = parseWorkspaceIdentity(numbered);
  REQUIRE(workspaceMatchesIdentifier("3", *numberedIdentity, "3"));
  REQUIRE_FALSE(workspaceMatchesIdentifier("4", *numberedIdentity, "3"));
}
