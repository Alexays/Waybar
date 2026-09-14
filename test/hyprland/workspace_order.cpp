#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <algorithm>
#include <string>
#include <vector>

#include "modules/hyprland/workspace_identity.hpp"

namespace hyprland = waybar::modules::hyprland;
using hyprland::WorkspaceIdentity;
using hyprland::WorkspaceKind;
using hyprland::workspaceKindForAddress;
using hyprland::workspaceLessByDefault;
using hyprland::workspaceLessById;
using hyprland::workspaceTypeName;

namespace {

// A workspace as the comparator sees it: its identity plus its display name.
struct Entry {
  WorkspaceIdentity identity;
  std::string name;
};

Entry numbered(int number, const std::string& name) {
  return {WorkspaceIdentity{std::to_string(number), WorkspaceKind::Numbered, number}, name};
}

// A Numbered workspace whose address did not yield a number -- Hyprland can
// label an out-of-int-range address "numbered".
Entry numberless(const std::string& address, const std::string& name) {
  return {WorkspaceIdentity{address, WorkspaceKind::Numbered, std::nullopt}, name};
}

Entry named(const std::string& address) {
  return {WorkspaceIdentity{address, WorkspaceKind::Named, std::nullopt}, address};
}

Entry special(const std::string& address, const std::string& name) {
  return {WorkspaceIdentity{address, WorkspaceKind::Special, std::nullopt}, name};
}

using Comparator = bool (*)(const WorkspaceIdentity&, const std::string&, const WorkspaceIdentity&,
                            const std::string&);

bool less(Comparator cmp, const Entry& a, const Entry& b) {
  return cmp(a.identity, a.name, b.identity, b.name);
}

bool equiv(Comparator cmp, const Entry& a, const Entry& b) {
  return !less(cmp, a, b) && !less(cmp, b, a);
}

// Exhaustively verify the axioms `std::ranges::sort` relies on; violating any
// of them is undefined behavior, not merely a wrong order.
void requireStrictWeakOrdering(Comparator cmp, const std::vector<Entry>& entries) {
  bool sawTie = false;

  for (const auto& a : entries) {
    // Irreflexivity.
    REQUIRE_FALSE(less(cmp, a, a));

    for (const auto& b : entries) {
      // Asymmetry.
      REQUIRE_FALSE((less(cmp, a, b) && less(cmp, b, a)));
      sawTie = sawTie || (&a != &b && equiv(cmp, a, b));

      for (const auto& c : entries) {
        // Transitivity of <.
        if (less(cmp, a, b) && less(cmp, b, c)) {
          REQUIRE(less(cmp, a, c));
        }
        // Transitivity of equivalence.
        if (equiv(cmp, a, b) && equiv(cmp, b, c)) {
          REQUIRE(equiv(cmp, a, c));
        }
      }
    }
  }

  // Without a tied pair the equivalence axiom above is vacuous.
  REQUIRE(sawTie);
}

// Mixes numbered workspaces that do and do not carry a number -- the case that
// used to cycle.
std::vector<Entry> mixedPopulation() {
  return {
      numbered(1, "1"),
      numbered(2, "2"),
      numbered(10, "10"),
      numberless("15", "15"),
      numberless("15b", "15"),  // ties with the entry above
      numberless("0x1", "0x1"),
      named("web"),
      named("code"),
      special("special:special", "special"),
      special("special:spotify", "spotify"),
  };
}

}  // namespace

TEST_CASE("reported comparison cycle is gone", "[workspace_order]") {
  // The old comparator gave A<B (numeric), B<U ("10"<"15") and U<A ("15"<"2").
  const auto a = numbered(2, "2");
  const auto b = numbered(10, "10");
  const auto u = numberless("15", "15");

  for (const Comparator cmp : {&workspaceLessById, &workspaceLessByDefault}) {
    REQUIRE(less(cmp, a, b));
    REQUIRE(less(cmp, b, u));
    REQUIRE_FALSE(less(cmp, u, a));  // was true, closing the cycle
    REQUIRE(less(cmp, a, u));        // transitivity now holds
  }
}

TEST_CASE("sort-by id is a strict weak ordering", "[workspace_order]") {
  requireStrictWeakOrdering(&workspaceLessById, mixedPopulation());
}

TEST_CASE("sort-by default is a strict weak ordering", "[workspace_order]") {
  requireStrictWeakOrdering(&workspaceLessByDefault, mixedPopulation());
}

TEST_CASE("default ordering groups numbered then named then special", "[workspace_order]") {
  std::vector<Entry> entries{
      special("special:spotify", "spotify"),
      named("web"),
      numbered(10, "10"),
      numbered(2, "2"),
  };

  std::ranges::sort(entries, [](const Entry& a, const Entry& b) {
    return workspaceLessByDefault(a.identity, a.name, b.identity, b.name);
  });

  std::vector<std::string> order;
  order.reserve(entries.size());
  for (const auto& entry : entries) {
    order.push_back(entry.name);
  }

  // Numbers compare numerically, not lexicographically: 2 before 10.
  REQUIRE(order == std::vector<std::string>{"2", "10", "web", "spotify"});
}

TEST_CASE("numbered workspaces without a number sort after those with one", "[workspace_order]") {
  const auto withNumber = numbered(99, "99");
  const auto withoutNumber = numberless("99999999999999", "huge");

  REQUIRE(less(&workspaceLessById, withNumber, withoutNumber));
  REQUIRE(less(&workspaceLessByDefault, withNumber, withoutNumber));
}

TEST_CASE("address classification", "[workspace_order]") {
  // Bare `special` is a real persistent-workspaces selector.
  REQUIRE(workspaceKindForAddress("special") == WorkspaceKind::Special);
  REQUIRE(workspaceKindForAddress("special:special") == WorkspaceKind::Special);
  REQUIRE(workspaceKindForAddress("special:spotify") == WorkspaceKind::Special);

  REQUIRE(workspaceKindForAddress("3") == WorkspaceKind::Numbered);
  REQUIRE(workspaceKindForAddress("42") == WorkspaceKind::Numbered);

  REQUIRE(workspaceKindForAddress("web") == WorkspaceKind::Named);
  REQUIRE(workspaceKindForAddress("3a") == WorkspaceKind::Named);
  REQUIRE(workspaceKindForAddress("-99") == WorkspaceKind::Named);
  REQUIRE(workspaceKindForAddress("") == WorkspaceKind::Named);
}

TEST_CASE("type names round-trip through the parser", "[workspace_order]") {
  // `setAddress` and the persistent-workspace synthesis hand these names back
  // to parseWorkspaceIdentity, so the two mappings have to agree.
  for (const auto kind : {WorkspaceKind::Numbered, WorkspaceKind::Named, WorkspaceKind::Special}) {
    Json::Value ws;
    ws["address"] = kind == WorkspaceKind::Numbered ? "7" : "web";
    ws["type"] = workspaceTypeName(kind);

    const auto identity = hyprland::parseWorkspaceIdentity(ws);
    REQUIRE(identity.has_value());
    REQUIRE(identity->kind == kind);
  }
}

TEST_CASE("deriving a kind from an address keeps Numbered workspaces numbered",
          "[workspace_order]") {
  // The invariant `setAddress` relies on: Numbered implies the parser can turn
  // the address into a number.
  for (const std::string address : {"1", "2", "10", "4242"}) {
    Json::Value ws;
    ws["address"] = address;
    ws["type"] = workspaceTypeName(workspaceKindForAddress(address));

    const auto identity = hyprland::parseWorkspaceIdentity(ws);
    REQUIRE(identity.has_value());
    REQUIRE(identity->kind == WorkspaceKind::Numbered);
    REQUIRE(identity->number.has_value());
  }
}
