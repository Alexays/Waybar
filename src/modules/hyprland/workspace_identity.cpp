#include "modules/hyprland/workspace_identity.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <string_view>

namespace waybar::modules::hyprland {

namespace {

constexpr std::string_view kSpecialPrefix{"special:"};
constexpr std::string_view kNamePrefix{"name:"};
constexpr std::string_view kGenericSpecial{"special"};

// Hyprland's special namespace is exactly `special` or anything under
// `special:`. A workspace merely *named* `specialfoo` is not special.
bool isSpecialName(std::string_view name) {
  return name == kGenericSpecial || name.starts_with(kSpecialPrefix);
}

std::optional<int> parseNumber(const std::string& value) {
  try {
    return std::stoi(value);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// hyprwm/Hyprland#16140 reported the kind as `numbered`, `named` or `special`.
// hyprwm/Hyprland#16269 collapsed that to `normal` or `special` and marks a
// numbered workspace by carrying an `id`, so both schemas are read here.
WorkspaceKind kindFromPayload(const Json::Value& workspace) {
  const auto type = workspace["type"].asString();
  if (type == "special") {
    return WorkspaceKind::Special;
  }
  if (type == "numbered") {
    return WorkspaceKind::Numbered;
  }
  if (type == "named") {
    return WorkspaceKind::Named;
  }
  return workspace["id"].isInt() ? WorkspaceKind::Numbered : WorkspaceKind::Named;
}

}  // namespace

const char* workspaceTypeName(WorkspaceKind kind) {
  switch (kind) {
    case WorkspaceKind::Numbered:
      return "numbered";
    case WorkspaceKind::Special:
      return "special";
    case WorkspaceKind::Named:
      return "named";
  }
  // The switch covers every enumerator, so -Wswitch fails the build if one is
  // added without a case. That is not the same as unreachable: a C++ scoped
  // enum can hold any value its underlying bits can represent, so a bad cast
  // lands here. Say so rather than returning a name that reads as legitimate.
  spdlog::error("Unknown WorkspaceKind {}", static_cast<int>(kind));
  return "unknown";
}

WorkspaceKind workspaceKindForAddress(const std::string& address) {
  if (isSpecialName(address)) {
    return WorkspaceKind::Special;
  }
  if (!address.empty() &&
      std::ranges::all_of(address, [](unsigned char c) { return std::isdigit(c) != 0; })) {
    return WorkspaceKind::Numbered;
  }
  return WorkspaceKind::Named;
}

std::string workspaceDisplayName(const std::string& rawName, WorkspaceKind kind) {
  if (kind == WorkspaceKind::Special && rawName.starts_with(kSpecialPrefix)) {
    return rawName.substr(kSpecialPrefix.size());
  }
  return rawName;
}

WorkspaceSelector::WorkspaceSelector(const std::string& selector) {
  // A bare prefix selects nothing, so it is left alone rather than yielding an
  // empty name that would match every workspace of that kind.
  if (selector.size() > kSpecialPrefix.size() && selector.starts_with(kSpecialPrefix)) {
    kind = WorkspaceKind::Special;
    name = selector.substr(kSpecialPrefix.size());
    return;
  }
  if (selector.size() > kNamePrefix.size() && selector.starts_with(kNamePrefix)) {
    kind = WorkspaceKind::Named;
    name = selector.substr(kNamePrefix.size());
    return;
  }
  kind = workspaceKindForAddress(selector);
  name = selector;
}

std::string WorkspaceSelector::rawName() const {
  if (kind == WorkspaceKind::Special) {
    return std::string{kSpecialPrefix} + name;
  }
  return name;
}

std::optional<WorkspaceIdentity> parseWorkspaceIdentity(const Json::Value& workspace) {
  if (!workspace.isObject()) {
    return std::nullopt;
  }
  const auto rawName = workspace["name"].asString();

  // Hyprland with addressable workspaces (hyprwm/Hyprland#16140).
  if (workspace["address"].isString()) {
    WorkspaceIdentity identity;
    identity.address = workspace["address"].asString();
    identity.kind = kindFromPayload(workspace);
    identity.name = workspaceDisplayName(rawName, identity.kind);
    return identity;
  }

  // Hyprland before addressable workspaces: kind was encoded in the id's sign.
  if (workspace["id"].isInt()) {
    const int id = workspace["id"].asInt();
    WorkspaceIdentity identity;
    identity.address = std::to_string(id);
    if (id > 0) {
      identity.kind = WorkspaceKind::Numbered;
    } else if (isSpecialName(rawName)) {
      identity.kind = WorkspaceKind::Special;
    } else {
      identity.kind = WorkspaceKind::Named;
    }
    identity.name = workspaceDisplayName(rawName, identity.kind);
    return identity;
  }

  return std::nullopt;
}

std::optional<int> WorkspaceIdentity::number() const {
  if (kind != WorkspaceKind::Numbered) {
    return std::nullopt;
  }
  return parseNumber(address);
}

void WorkspaceIdentity::renumber(const std::string& newAddress) {
  if (name == address) {
    name = newAddress;
  }
  address = newAddress;
  kind = workspaceKindForAddress(newAddress);
}

bool workspaceLessById(const WorkspaceIdentity& a, const WorkspaceIdentity& b) {
  const auto numberA = a.number();
  const auto numberB = b.number();
  // Splitting on `has_value` before comparing numbers is what keeps this
  // transitive.
  if (numberA.has_value() != numberB.has_value()) {
    return numberA.has_value();
  }
  if (numberA.has_value()) {
    return *numberA < *numberB;
  }
  const int rankA = workspaceKindRank(a.kind);
  const int rankB = workspaceKindRank(b.kind);
  return rankA != rankB ? rankA < rankB : a.name < b.name;
}

bool workspaceLessByDefault(const WorkspaceIdentity& a, const WorkspaceIdentity& b) {
  const int rankA = workspaceKindRank(a.kind);
  const int rankB = workspaceKindRank(b.kind);
  if (rankA != rankB) {
    return rankA < rankB;
  }
  const auto numberA = a.number();
  const auto numberB = b.number();
  if (numberA.has_value() != numberB.has_value()) {
    return numberA.has_value();
  }
  if (numberA.has_value()) {
    return *numberA < *numberB;
  }
  return a.name < b.name;
}

}  // namespace waybar::modules::hyprland
