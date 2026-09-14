#include "modules/hyprland/workspace_identity.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace waybar::modules::hyprland {

namespace {

std::optional<int> parseNumber(const std::string& value) {
  try {
    return std::stoi(value);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

WorkspaceKind kindFromTypeName(std::string_view type) {
  if (type == "numbered") {
    return WorkspaceKind::Numbered;
  }
  if (type == "special") {
    return WorkspaceKind::Special;
  }
  return WorkspaceKind::Named;
}

}  // namespace

std::optional<WorkspaceIdentity> parseWorkspaceIdentity(const Json::Value& workspace) {
  if (!workspace.isObject()) {
    return std::nullopt;
  }

  // Hyprland with addressable workspaces (hyprwm/Hyprland#16140).
  if (workspace["address"].isString()) {
    WorkspaceIdentity identity;
    identity.address = workspace["address"].asString();
    identity.kind = kindFromTypeName(workspace["type"].asString());
    if (identity.kind == WorkspaceKind::Numbered) {
      identity.number = parseNumber(identity.address);
    }
    return identity;
  }

  // Hyprland before addressable workspaces: kind was encoded in the id's sign.
  if (workspace["id"].isInt()) {
    const int id = workspace["id"].asInt();
    WorkspaceIdentity identity;
    identity.address = std::to_string(id);
    if (id > 0) {
      identity.kind = WorkspaceKind::Numbered;
      identity.number = id;
    } else if (workspace["name"].asString().starts_with("special")) {
      identity.kind = WorkspaceKind::Special;
    } else {
      identity.kind = WorkspaceKind::Named;
    }
    return identity;
  }

  return std::nullopt;
}

const char* workspaceTypeName(WorkspaceKind kind) {
  switch (kind) {
    case WorkspaceKind::Numbered:
      return "numbered";
    case WorkspaceKind::Special:
      return "special";
    case WorkspaceKind::Named:
      break;
  }
  return "named";
}

WorkspaceKind workspaceKindForAddress(const std::string& address) {
  // Loose "special" prefix, not "special:": a bare `special` is a valid
  // persistent-workspaces selector, and the legacy branch above matches names
  // the same way.
  if (address.starts_with("special")) {
    return WorkspaceKind::Special;
  }
  if (!address.empty() &&
      std::ranges::all_of(address, [](unsigned char c) { return std::isdigit(c) != 0; })) {
    return WorkspaceKind::Numbered;
  }
  return WorkspaceKind::Named;
}

bool workspaceSelectorMatchesName(const std::string& selector, const std::string& name) {
  // TODO: support the rest of the selector grammar.
  // https://wiki.hyprland.org/Configuring/Workspace-Rules/#workspace-selectors
  std::string_view wanted{selector};
  if (wanted.starts_with("special:")) {
    wanted.remove_prefix(std::string_view{"special:"}.size());
  } else if (wanted.starts_with("name:")) {
    wanted.remove_prefix(std::string_view{"name:"}.size());
  }
  return !wanted.empty() && wanted == name;
}

int workspaceKindRank(WorkspaceKind kind) {
  switch (kind) {
    case WorkspaceKind::Numbered:
      return 0;
    case WorkspaceKind::Named:
      return 1;
    case WorkspaceKind::Special:
      return 2;
  }
  return 1;
}

bool workspaceLessById(const WorkspaceIdentity& a, const std::string& aName,
                       const WorkspaceIdentity& b, const std::string& bName) {
  // Splitting on `has_value` before comparing numbers is what keeps this
  // transitive.
  if (a.number.has_value() != b.number.has_value()) {
    return a.number.has_value();
  }
  if (a.number.has_value()) {
    return *a.number < *b.number;
  }
  const int rankA = workspaceKindRank(a.kind);
  const int rankB = workspaceKindRank(b.kind);
  return rankA != rankB ? rankA < rankB : aName < bName;
}

bool workspaceLessByDefault(const WorkspaceIdentity& a, const std::string& aName,
                            const WorkspaceIdentity& b, const std::string& bName) {
  const int rankA = workspaceKindRank(a.kind);
  const int rankB = workspaceKindRank(b.kind);
  if (rankA != rankB) {
    return rankA < rankB;
  }
  if (a.number.has_value() != b.number.has_value()) {
    return a.number.has_value();
  }
  if (a.number.has_value()) {
    return *a.number < *b.number;
  }
  return aName < bName;
}

}  // namespace waybar::modules::hyprland
