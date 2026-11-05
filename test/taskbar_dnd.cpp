#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <cstdint>
#include <utility>
#include <vector>

#include "modules/wlr/taskbar_dnd.hpp"

TEST_CASE("taskbar drag resolves live task positions") {
  const std::vector<uint32_t> task_ids{10, 20, 30};

  REQUIRE(waybar::modules::wlr::dnd::reorder_positions(task_ids, 10, 30) ==
          std::pair<std::size_t, std::size_t>{0, 2});
  REQUIRE(waybar::modules::wlr::dnd::reorder_positions(task_ids, 20, 20) ==
          std::pair<std::size_t, std::size_t>{1, 1});
}

TEST_CASE("taskbar drag ignores tasks that are no longer live") {
  const std::vector<uint32_t> task_ids{10, 30};

  REQUIRE_FALSE(waybar::modules::wlr::dnd::reorder_positions(task_ids, 20, 30));
  REQUIRE_FALSE(waybar::modules::wlr::dnd::reorder_positions(task_ids, 10, 20));
}
