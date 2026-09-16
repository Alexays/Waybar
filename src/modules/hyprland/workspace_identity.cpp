#include "modules/hyprland/workspace_identity.hpp"

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
    } else if (isSpecialName(workspace["name"].asString())) {
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
      return "named";
  }
  // Unreachable: the switch covers every WorkspaceKind, so -Wswitch fails the
  // build if an enumerator is added without a case above. This exists only to
  // satisfy -Wreturn-type.
  return "named";
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

bool isGenericSpecialName(const std::string& rawName, WorkspaceKind kind) {
  return kind == WorkspaceKind::Special && rawName == kGenericSpecial;
}

WorkspaceSelector parseWorkspaceSelector(const std::string& selector) {
  // A bare prefix selects nothing, so it is left alone rather than yielding an
  // empty name that would match every workspace of that kind.
  if (selector.size() > kSpecialPrefix.size() && selector.starts_with(kSpecialPrefix)) {
    return {WorkspaceKind::Special, selector.substr(kSpecialPrefix.size())};
  }
  if (selector.size() > kNamePrefix.size() && selector.starts_with(kNamePrefix)) {
    return {WorkspaceKind::Named, selector.substr(kNamePrefix.size())};
  }
  const auto kind = workspaceKindForAddress(selector);
  return {kind, selector, isGenericSpecialName(selector, kind)};
}

bool workspaceMatchesIdentifier(const std::string& identifier, const WorkspaceIdentity& identity,
                                const std::string& rawName) {
  if (identity.address == identifier) {
    return true;
  }
  // Hyprland's workspace events carry the identifier the workspace was created
  // with, not the address `workspaces` reports. They agree for numbered and
  // special workspaces, but a named one created as `name:web` is announced as
  // `name:web` and reported at address `web`, so the raw comparison misses it.
  const auto selector = parseWorkspaceSelector(identifier);
  return identity.kind == selector.kind &&
         isGenericSpecialName(rawName, identity.kind) == selector.isGenericSpecial &&
         workspaceDisplayName(rawName, identity.kind) == selector.name;
}

std::string workspaceRawName(const WorkspaceSelector& selector) {
  if (selector.kind == WorkspaceKind::Special && !selector.isGenericSpecial) {
    return std::string{kSpecialPrefix} + selector.name;
  }
  return selector.name;
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
