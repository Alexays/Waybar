#pragma once

#include <json/value.h>

#include <optional>
#include <string>

namespace waybar::modules::hyprland {

// The declaration order is load-bearing: it is the display order the
// workspaces module groups by, which workspaceKindRank() reads off directly.
enum class WorkspaceKind { Numbered, Named, Special };

// A workspace's stable key plus its kind, normalized across Hyprland IPC
// schema versions. `number` is set only for Numbered workspaces.
struct WorkspaceIdentity {
  std::string address;
  WorkspaceKind kind = WorkspaceKind::Named;
  std::optional<int> number;

  bool operator==(const WorkspaceIdentity& other) const { return address == other.address; }
};

// A `persistent-workspaces` entry as the user wrote it, split into the kind it
// selects and the workspace name it selects.
struct WorkspaceSelector {
  WorkspaceKind kind = WorkspaceKind::Named;
  std::string name;
  // The generic special workspace, written as a bare `special`. A selector of
  // `special:special` names the user's own special workspace called `special`,
  // which displays identically but is a different workspace.
  bool isGenericSpecial = false;
};

// Normalizes a workspace object from `workspaces`, `activeworkspace`, or a
// client's nested "workspace" value. Returns nullopt when the payload carries
// neither schema's identity fields.
std::optional<WorkspaceIdentity> parseWorkspaceIdentity(const Json::Value& workspace);

// The literal Hyprland IPC `type` name for a kind. Inverse of the `type`
// mapping `parseWorkspaceIdentity` applies.
const char* workspaceTypeName(WorkspaceKind kind);

// Classifies a bare address for the one path that has no IPC `type` field to
// read: the `workspacev2`-style rename that moves a workspace between the
// numbered and named namespaces.
WorkspaceKind workspaceKindForAddress(const std::string& address);

// The name to display for a workspace whose IPC payload reported `rawName`.
//
// Hyprland namespaces a special workspace as `special:<name>`, and exactly one
// such prefix is removed -- so a special workspace the user named `special:123`
// is reported as `special:special:123` and displays as `special:123`. The
// generic special workspace is reported as plain `special` and keeps that name.
//
// No other kind carries a prefix. In particular `name:` is selector syntax that
// Hyprland never prepends to a reported name, so a named workspace actually
// called `name:foo` displays verbatim.
std::string workspaceDisplayName(const std::string& rawName, WorkspaceKind kind);

// True when `rawName` denotes the generic special workspace -- the one
// `togglespecialworkspace` toggles with an empty argument. It is
// distinguishable from a special workspace the user named `special` (reported
// as `special:special`) only before the namespace prefix is removed, so this
// takes the raw name rather than the display name.
bool isGenericSpecialName(const std::string& rawName, WorkspaceKind kind);

// Splits a `persistent-workspaces` selector into kind and name.
//   "7"              -> {Numbered, "7"}
//   "web"            -> {Named,    "web"}
//   "name:web"       -> {Named,    "web"}
//   "special"        -> {Special,  "special", generic}
//   "special:spotify"-> {Special,  "spotify"}
//   "special:special"-> {Special,  "special"}
// TODO: support the rest of the selector grammar.
// https://wiki.hyprland.org/Configuring/Workspace-Rules/#workspace-selectors
WorkspaceSelector parseWorkspaceSelector(const std::string& selector);

// The raw IPC name Hyprland would report for the workspace `selector` selects.
// Inverse of workspaceDisplayName(), used to build the placeholder payload for
// a persistent-workspaces entry so that the Workspace constructor only ever
// sees IPC-shaped names.
std::string workspaceRawName(const WorkspaceSelector& selector);

// Display grouping for the workspaces module: numbered -> named -> special.
// Taken from WorkspaceKind's declaration order rather than a second, hand-kept
// table that could drift from it.
constexpr int workspaceKindRank(WorkspaceKind kind) { return static_cast<int>(kind); }

// The `sort-by: id` and `sort-by: default` orderings. Both are strict weak
// orderings even for a Numbered workspace whose address yielded no number.
bool workspaceLessById(const WorkspaceIdentity& a, const std::string& aName,
                       const WorkspaceIdentity& b, const std::string& bName);
bool workspaceLessByDefault(const WorkspaceIdentity& a, const std::string& aName,
                            const WorkspaceIdentity& b, const std::string& bName);

}  // namespace waybar::modules::hyprland
