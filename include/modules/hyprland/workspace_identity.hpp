#pragma once

#include <json/value.h>

#include <optional>
#include <string>
#include <utility>

namespace waybar::modules::hyprland {

// The declaration order is load-bearing: it is the display order the
// workspaces module groups by, which workspaceKindRank() reads off directly.
enum class WorkspaceKind { Numbered, Named, Special };

// The hyprwm/Hyprland#16140 IPC `type` name for a kind, which spells out all
// three kinds. Hyprland has since collapsed numbered and named into `normal`,
// but parseWorkspaceIdentity() still reads these, so they remain the unambiguous
// way to build a payload that has no `id`.
const char* workspaceTypeName(WorkspaceKind kind);

// Classifies a bare string for the paths that have no IPC `type` field to read:
// the `workspacev2`-style rename that moves a workspace between the numbered
// and named namespaces, and `persistent-workspaces` entries.
WorkspaceKind workspaceKindForAddress(const std::string& address);

// The name to display for a workspace whose IPC payload reported `rawName`.
//
// Hyprland namespaces a special workspace as `special:<name>`, and exactly one
// such prefix is removed -- so a special workspace the user named `special:123`
// is reported as `special:special:123` and displays as `special:123`.
//
// This is not idempotent, and is not meant to be: it maps a raw IPC name to a
// display name once. Nothing calls it on a name that has already been through
// it -- WorkspaceIdentity stores the display name, so every later reader takes
// it from there. The kind is still checked alongside the prefix because a
// Hyprland older than 0.41 reported the generic special workspace as a bare
// `special`, with no prefix to strip.
//
// No other kind carries a prefix. In particular `name:` is selector syntax that
// Hyprland never prepends to a reported name, so a named workspace actually
// called `name:foo` displays verbatim.
std::string workspaceDisplayName(const std::string& rawName, WorkspaceKind kind);

// A workspace named the way a user writes it -- a `persistent-workspaces` entry
// or the identifier Hyprland puts in an event payload -- split into the kind it
// selects and the workspace name it selects.
struct WorkspaceSelector {
  WorkspaceKind kind = WorkspaceKind::Named;
  std::string name;

  WorkspaceSelector(WorkspaceKind kind, std::string name) : kind(kind), name(std::move(name)) {}

  // Parses selector syntax:
  //   "7"               -> {Numbered, "7"}
  //   "web"             -> {Named,    "web"}
  //   "name:web"        -> {Named,    "web"}
  //   "special"         -> {Special,  "special"}
  //   "special:spotify" -> {Special,  "spotify"}
  // Every string names some workspace, so this parse is total and belongs in a
  // constructor. Contrast parseWorkspaceIdentity(), which can fail.
  // TODO: support the rest of the selector grammar.
  // https://wiki.hyprland.org/Configuring/Workspace-Rules/#workspace-selectors
  explicit WorkspaceSelector(const std::string& selector);

  // The raw IPC name Hyprland reports for the workspace this selects. Inverse
  // of workspaceDisplayName(), used to build the placeholder payload for a
  // persistent-workspaces entry so that the Workspace constructor only ever
  // sees IPC-shaped names.
  std::string rawName() const;
};

// A workspace's stable key, display name and kind, normalized across Hyprland
// IPC schema versions.
struct WorkspaceIdentity {
  std::string address;
  std::string name;
  WorkspaceKind kind = WorkspaceKind::Named;

  // Read off the address rather than stored alongside it, so the two cannot
  // disagree. Empty unless this workspace is Numbered.
  std::optional<int> number() const;

  WorkspaceSelector asSelector() const { return {kind, name}; }

  // Applies a `changeworkspaceid` event, which Hyprland only emits for a
  // numbered workspace. Like Hyprland, a name set by `renameworkspace` is kept
  // and a name still equal to the old number follows the new one.
  void renumber(const std::string& newAddress);

  bool matches(const WorkspaceSelector& selector) const {
    // The kind is what separates a named workspace `foo` from a special one
    // `special:foo`: both display `foo`.
    return kind == selector.kind && name == selector.name;
  }

  // True when `identifier` -- the workspace field of an IPC event payload, or a
  // `persistent-workspaces` entry -- refers to this workspace. The address
  // alone is not enough: Hyprland announces a named workspace by the `name:foo`
  // selector it was created with while reporting its address as `foo`.
  bool matches(const std::string& identifier) const {
    return address == identifier || matches(WorkspaceSelector{identifier});
  }

  bool operator==(const WorkspaceIdentity& other) const { return address == other.address; }
};

// Normalizes a workspace object from `workspaces`, `activeworkspace`, or a
// client's nested "workspace" value. Returns nullopt when the payload carries
// neither schema's identity fields -- a parse that can fail, so a factory
// rather than a constructor.
std::optional<WorkspaceIdentity> parseWorkspaceIdentity(const Json::Value& workspace);

// Display grouping for the workspaces module: numbered -> named -> special.
// Taken from WorkspaceKind's declaration order rather than a second, hand-kept
// table that could drift from it.
constexpr int workspaceKindRank(WorkspaceKind kind) { return static_cast<int>(kind); }

// The `sort-by: id` and `sort-by: default` orderings. Both are strict weak
// orderings even for a Numbered workspace whose address yielded no number.
bool workspaceLessById(const WorkspaceIdentity& a, const WorkspaceIdentity& b);
bool workspaceLessByDefault(const WorkspaceIdentity& a, const WorkspaceIdentity& b);

}  // namespace waybar::modules::hyprland
