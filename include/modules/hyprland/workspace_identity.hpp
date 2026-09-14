#pragma once

#include <json/value.h>

#include <optional>
#include <string>

namespace waybar::modules::hyprland {

enum class WorkspaceKind { Numbered, Named, Special };

// A workspace's stable key plus its kind, normalized across Hyprland IPC
// schema versions. `number` is set only for Numbered workspaces.
struct WorkspaceIdentity {
  std::string address;
  WorkspaceKind kind = WorkspaceKind::Named;
  std::optional<int> number;

  bool operator==(const WorkspaceIdentity& other) const { return address == other.address; }
};

// Normalizes a workspace object from `workspaces`, `activeworkspace`, or a
// client's nested "workspace" value. Returns nullopt when the payload carries
// neither schema's identity fields.
std::optional<WorkspaceIdentity> parseWorkspaceIdentity(const Json::Value& workspace);

// The literal Hyprland IPC `type` name for a kind. Inverse of the `type`
// mapping `parseWorkspaceIdentity` applies.
const char* workspaceTypeName(WorkspaceKind kind);

// Classifies a bare address, or a persistent-workspace selector a user typed,
// for the two paths that have no IPC `type` field to read.
WorkspaceKind workspaceKindForAddress(const std::string& address);

// True when `selector` -- a workspace selector as written in
// `persistent-workspaces`, or a name from an IPC payload -- refers to the
// workspace displayed as `name`.
bool workspaceSelectorMatchesName(const std::string& selector, const std::string& name);

// Display grouping for the workspaces module: numbered -> named -> special.
int workspaceKindRank(WorkspaceKind kind);

// The `sort-by: id` and `sort-by: default` orderings. Both are strict weak
// orderings even for a Numbered workspace whose address yielded no number.
bool workspaceLessById(const WorkspaceIdentity& a, const std::string& aName,
                       const WorkspaceIdentity& b, const std::string& bName);
bool workspaceLessByDefault(const WorkspaceIdentity& a, const std::string& aName,
                            const WorkspaceIdentity& b, const std::string& bName);

}  // namespace waybar::modules::hyprland
