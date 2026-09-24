#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "modules/hyprland/workspace_identity.hpp"

namespace hyprland = waybar::modules::hyprland;
using hyprland::parseWorkspaceIdentity;
using hyprland::workspaceDisplayName;
using hyprland::WorkspaceKind;
using hyprland::WorkspaceSelector;

TEST_CASE("addressable numbered workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["address"] = "3";
  ws["type"] = "numbered";
  ws["name"] = "3";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "3");
  REQUIRE(identity->name == "3");
  REQUIRE(identity->kind == WorkspaceKind::Numbered);
  REQUIRE(identity->number() == 3);
}

TEST_CASE("addressable special workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["address"] = "special:spotify";
  ws["type"] = "special";
  ws["name"] = "special:spotify";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "special:spotify");
  REQUIRE(identity->name == "spotify");
  REQUIRE(identity->kind == WorkspaceKind::Special);
  REQUIRE_FALSE(identity->number().has_value());
}

TEST_CASE("addressable named workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["address"] = "web";
  ws["type"] = "named";
  ws["name"] = "web";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "web");
  REQUIRE(identity->name == "web");
  REQUIRE(identity->kind == WorkspaceKind::Named);
  REQUIRE_FALSE(identity->number().has_value());
}

TEST_CASE("normal/special schema from hyprwm/Hyprland#16269", "[workspace_identity]") {
  // Taken from `hyprctl workspaces -j` on v0.56.0-198: numbered and named
  // workspaces are both `normal`, told apart by whether `id` is present.
  Json::Value numbered;
  numbered["address"] = "10";
  numbered["id"] = 10;
  numbered["type"] = "normal";
  numbered["name"] = "10";
  const auto numberedIdentity = parseWorkspaceIdentity(numbered);
  REQUIRE(numberedIdentity.has_value());
  REQUIRE(numberedIdentity->kind == WorkspaceKind::Numbered);
  REQUIRE(numberedIdentity->number() == 10);

  Json::Value named;
  named["address"] = "web";
  named["type"] = "normal";
  named["name"] = "web";
  const auto namedIdentity = parseWorkspaceIdentity(named);
  REQUIRE(namedIdentity.has_value());
  REQUIRE(namedIdentity->kind == WorkspaceKind::Named);
  REQUIRE_FALSE(namedIdentity->number().has_value());

  Json::Value special;
  special["address"] = "special:magic";
  special["type"] = "special";
  special["name"] = "special:magic";
  const auto specialIdentity = parseWorkspaceIdentity(special);
  REQUIRE(specialIdentity.has_value());
  REQUIRE(specialIdentity->kind == WorkspaceKind::Special);
  REQUIRE(specialIdentity->name == "magic");
}

TEST_CASE("renumbering follows Hyprland's naming", "[workspace_identity]") {
  Json::Value ws;
  ws["address"] = "3";
  ws["id"] = 3;
  ws["type"] = "normal";
  ws["name"] = "3";

  auto unrenamed = parseWorkspaceIdentity(ws);
  REQUIRE(unrenamed.has_value());
  unrenamed->renumber("5");
  REQUIRE(unrenamed->address == "5");
  REQUIRE(unrenamed->name == "5");
  REQUIRE(unrenamed->number() == 5);

  // A name set by renameworkspace survives the renumbering.
  ws["name"] = "code";
  auto renamed = parseWorkspaceIdentity(ws);
  REQUIRE(renamed.has_value());
  renamed->renumber("5");
  REQUIRE(renamed->address == "5");
  REQUIRE(renamed->name == "code");
  REQUIRE(renamed->number() == 5);
}

TEST_CASE("legacy numeric workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["id"] = 2;
  ws["name"] = "2";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->address == "2");
  REQUIRE(identity->kind == WorkspaceKind::Numbered);
  REQUIRE(identity->number() == 2);
}

TEST_CASE("legacy special workspace", "[workspace_identity]") {
  // Hyprland has reported the generic special workspace as `special:special`
  // since 0.41; before that it was a bare `special`. Both are the same
  // workspace and both display as `special`.
  for (const auto* rawName : {"special:special", "special"}) {
    Json::Value ws;
    ws["id"] = -99;
    ws["name"] = rawName;

    auto identity = parseWorkspaceIdentity(ws);

    INFO("raw name: " << rawName);
    REQUIRE(identity.has_value());
    REQUIRE(identity->address == "-99");
    REQUIRE(identity->kind == WorkspaceKind::Special);
    REQUIRE(identity->name == "special");
  }
}

TEST_CASE("legacy named workspace", "[workspace_identity]") {
  Json::Value ws;
  ws["id"] = -1377;
  ws["name"] = "web";

  auto identity = parseWorkspaceIdentity(ws);

  REQUIRE(identity.has_value());
  REQUIRE(identity->kind == WorkspaceKind::Named);
  REQUIRE(identity->name == "web");
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
  REQUIRE(identity->name == "specialfoo");
}

TEST_CASE("display name strips exactly one special prefix", "[workspace_identity]") {
  REQUIRE(workspaceDisplayName("special:spotify", WorkspaceKind::Special) == "spotify");

  // A special workspace the user named `special:123` is reported by Hyprland as
  // `special:special:123`, so only the outer prefix belongs to the namespace.
  REQUIRE(workspaceDisplayName("special:special:123", WorkspaceKind::Special) == "special:123");

  // The generic special workspace: `special:special` since Hyprland 0.41, a
  // bare `special` before that. Both display as `special`.
  REQUIRE(workspaceDisplayName("special:special", WorkspaceKind::Special) == "special");
  REQUIRE(workspaceDisplayName("special", WorkspaceKind::Special) == "special");

  // The kind is checked alongside the prefix, so a named workspace keeps a name
  // that merely looks like the namespace.
  REQUIRE(workspaceDisplayName("special:foo", WorkspaceKind::Named) == "special:foo");
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
  auto selector = WorkspaceSelector{"7"};
  REQUIRE(selector.kind == WorkspaceKind::Numbered);
  REQUIRE(selector.name == "7");

  selector = WorkspaceSelector{"web"};
  REQUIRE(selector.kind == WorkspaceKind::Named);
  REQUIRE(selector.name == "web");

  selector = WorkspaceSelector{"name:web"};
  REQUIRE(selector.kind == WorkspaceKind::Named);
  REQUIRE(selector.name == "web");

  selector = WorkspaceSelector{"special"};
  REQUIRE(selector.kind == WorkspaceKind::Special);
  REQUIRE(selector.name == "special");

  selector = WorkspaceSelector{"special:spotify"};
  REQUIRE(selector.kind == WorkspaceKind::Special);
  REQUIRE(selector.name == "spotify");

  // A named workspace `foo` and a special one `special:foo` display the same
  // name; only the kind separates them.
  REQUIRE(WorkspaceSelector{"foo"}.kind == WorkspaceKind::Named);
  REQUIRE(WorkspaceSelector{"special:foo"}.kind == WorkspaceKind::Special);
  REQUIRE(WorkspaceSelector{"foo"}.name == WorkspaceSelector{"special:foo"}.name);

  // `specialfoo` is not under the special namespace.
  REQUIRE(WorkspaceSelector{"specialfoo"}.kind == WorkspaceKind::Named);
}

TEST_CASE("a bare selector prefix selects nothing", "[workspace_identity]") {
  // Stripping these would leave an empty name that matches every workspace of
  // the kind, so they are left intact and simply match nothing.
  REQUIRE(WorkspaceSelector{"special:"}.name == "special:");
  REQUIRE(WorkspaceSelector{"name:"}.name == "name:");
  REQUIRE(WorkspaceSelector{""}.name.empty());
}

TEST_CASE("raw name round-trips through the display name", "[workspace_identity]") {
  for (const auto* selectorText : {"7", "web", "name:web", "special", "special:spotify",
                                   "special:special", "special:name:foo", "specialfoo"}) {
    const WorkspaceSelector selector{selectorText};
    const auto rawName = selector.rawName();
    INFO("selector: " << selectorText << " raw: " << rawName);
    REQUIRE(workspaceDisplayName(rawName, selector.kind) == selector.name);
  }
}

TEST_CASE("raw name restores the namespace Hyprland would report", "[workspace_identity]") {
  REQUIRE(WorkspaceSelector{"special:spotify"}.rawName() == "special:spotify");
  REQUIRE(WorkspaceSelector{"name:web"}.rawName() == "web");
  REQUIRE(WorkspaceSelector{"7"}.rawName() == "7");

  // `special` and `special:special` both select the generic special workspace,
  // which Hyprland reports as `special:special`.
  REQUIRE(WorkspaceSelector{"special"}.rawName() == "special:special");
  REQUIRE(WorkspaceSelector{"special:special"}.rawName() == "special:special");
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

  REQUIRE(namedIdentity->matches("name:web"));
  REQUIRE(namedIdentity->matches("web"));
  REQUIRE_FALSE(namedIdentity->matches("name:other"));
  // A special workspace displaying the same name is a different workspace.
  REQUIRE_FALSE(namedIdentity->matches("special:web"));

  Json::Value special;
  special["address"] = "special:newspecial";
  special["type"] = "special";
  special["name"] = "special:newspecial";
  const auto specialIdentity = parseWorkspaceIdentity(special);
  REQUIRE(specialIdentity.has_value());

  REQUIRE(specialIdentity->matches("special:newspecial"));
  REQUIRE_FALSE(specialIdentity->matches("newspecial"));

  Json::Value numbered;
  numbered["address"] = "3";
  numbered["type"] = "numbered";
  numbered["name"] = "3";
  const auto numberedIdentity = parseWorkspaceIdentity(numbered);
  REQUIRE(numberedIdentity.has_value());
  REQUIRE(numberedIdentity->matches("3"));
  REQUIRE_FALSE(numberedIdentity->matches("4"));
}

TEST_CASE("a persistent-workspaces entry finds the live generic special workspace",
          "[workspace_identity]") {
  // `persistent-workspaces: { "special": ... }` builds its placeholder from the
  // selector; the live workspace arrives as `special:special`. The two have to
  // resolve to each other or the placeholder never adopts the live address.
  Json::Value live;
  live["address"] = "special:special";
  live["type"] = "special";
  live["name"] = "special:special";
  const auto liveIdentity = parseWorkspaceIdentity(live);
  REQUIRE(liveIdentity.has_value());

  REQUIRE(liveIdentity->matches(WorkspaceSelector{"special"}));
  REQUIRE(liveIdentity->matches(WorkspaceSelector{"special:special"}));

  // The placeholder is built in the shape Hyprland reports, so it parses to the
  // same identity as the live workspace bar its address.
  Json::Value placeholder;
  const WorkspaceSelector selector{"special"};
  placeholder["address"] = "special";
  placeholder["type"] = "special";
  placeholder["name"] = selector.rawName();
  const auto placeholderIdentity = parseWorkspaceIdentity(placeholder);
  REQUIRE(placeholderIdentity.has_value());
  REQUIRE(placeholderIdentity->matches(liveIdentity->asSelector()));
}
