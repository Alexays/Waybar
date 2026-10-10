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

WorkspaceIdentity numbered(int number) {
  return {std::to_string(number), std::to_string(number), WorkspaceKind::Numbered};
}

// A Numbered workspace whose address does not yield a number -- Hyprland can
// label an out-of-int-range address "numbered". The number is derived from the
// address, so this is the only way to produce one.
WorkspaceIdentity numberless(const std::string& address, const std::string& name) {
  return {address, name, WorkspaceKind::Numbered};
}

WorkspaceIdentity named(const std::string& address) {
  return {address, address, WorkspaceKind::Named};
}

WorkspaceIdentity special(const std::string& address, const std::string& name) {
  return {address, name, WorkspaceKind::Special};
}

using Comparator = bool (*)(const WorkspaceIdentity&, const WorkspaceIdentity&);

bool equiv(Comparator cmp, const WorkspaceIdentity& a, const WorkspaceIdentity& b) {
  return !cmp(a, b) && !cmp(b, a);
}

// Exhaustively verify the axioms `std::ranges::sort` relies on; violating any
// of them is undefined behavior, not merely a wrong order.
void requireStrictWeakOrdering(Comparator cmp, const std::vector<WorkspaceIdentity>& entries) {
  bool sawTie = false;

  for (const auto& a : entries) {
    // Irreflexivity.
    REQUIRE_FALSE(cmp(a, a));

    for (const auto& b : entries) {
      // Asymmetry.
      REQUIRE_FALSE((cmp(a, b) && cmp(b, a)));
      sawTie = sawTie || (&a != &b && equiv(cmp, a, b));

      for (const auto& c : entries) {
        // Transitivity of <.
        if (cmp(a, b) && cmp(b, c)) {
          REQUIRE(cmp(a, c));
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
std::vector<WorkspaceIdentity> mixedPopulation() {
  return {
      numbered(1),
      numbered(2),
      numbered(10),
      numberless("99999999999999", "15"),
      numberless("99999999999998", "15"),  // ties with the entry above
      numberless("huge", "huge"),
      named("web"),
      named("code"),
      special("special:special", "special"),
      special("special:spotify", "spotify"),
  };
}

}  // namespace

TEST_CASE("a number is derived from the address only for numbered workspaces",
          "[workspace_order]") {
  REQUIRE(numbered(7).number() == 7);
  REQUIRE_FALSE(numberless("99999999999999", "15").number().has_value());

  // A legacy named or special workspace carries a negative id as its address;
  // that is a key, not a position in the numbered namespace.
  REQUIRE_FALSE(named("-1377").number().has_value());
  REQUIRE_FALSE(special("-99", "special").number().has_value());
}

TEST_CASE("reported comparison cycle is gone", "[workspace_order]") {
  // The old comparator gave A<B (numeric), B<U ("10"<"15") and U<A ("15"<"2").
  const auto a = numbered(2);
  const auto b = numbered(10);
  const auto u = numberless("99999999999999", "15");

  for (const Comparator cmp : {&workspaceLessById, &workspaceLessByDefault}) {
    REQUIRE(cmp(a, b));
    REQUIRE(cmp(b, u));
    REQUIRE_FALSE(cmp(u, a));  // was true, closing the cycle
    REQUIRE(cmp(a, u));        // transitivity now holds
  }
}

TEST_CASE("sort-by id is a strict weak ordering", "[workspace_order]") {
  requireStrictWeakOrdering(&workspaceLessById, mixedPopulation());
}

TEST_CASE("sort-by default is a strict weak ordering", "[workspace_order]") {
  requireStrictWeakOrdering(&workspaceLessByDefault, mixedPopulation());
}

TEST_CASE("default ordering groups numbered then named then special", "[workspace_order]") {
  std::vector<WorkspaceIdentity> entries{
      special("special:spotify", "spotify"), named("web"), numbered(10), numbered(2), numbered(1),
  };

  std::ranges::sort(entries, workspaceLessByDefault);

  std::vector<std::string> order;
  order.reserve(entries.size());
  for (const auto& entry : entries) {
    order.push_back(entry.name);
  }

  // Numbers compare numerically, not lexicographically: 1 and 2 before 10.
  REQUIRE(order == std::vector<std::string>{"1", "2", "10", "web", "spotify"});
}

TEST_CASE("numbered workspaces without a number sort after those with one", "[workspace_order]") {
  const auto withNumber = numbered(99);
  const auto withoutNumber = numberless("99999999999999", "huge");

  REQUIRE(workspaceLessById(withNumber, withoutNumber));
  REQUIRE(workspaceLessByDefault(withNumber, withoutNumber));
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
  // The persistent-workspace synthesis hands these names back to
  // parseWorkspaceIdentity, so the two mappings have to agree.
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
  // The invariant `setAddress` relies on: Numbered implies the address yields a
  // number.
  for (const std::string address : {"1", "2", "10", "4242"}) {
    Json::Value ws;
    ws["address"] = address;
    ws["type"] = workspaceTypeName(workspaceKindForAddress(address));

    const auto identity = hyprland::parseWorkspaceIdentity(ws);
    REQUIRE(identity.has_value());
    REQUIRE(identity->kind == WorkspaceKind::Numbered);
    REQUIRE(identity->number().has_value());
  }
}
