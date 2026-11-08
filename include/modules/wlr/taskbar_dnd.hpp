#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace waybar::modules::wlr::dnd {
inline std::optional<std::pair<std::size_t, std::size_t>> reorder_positions(
    const std::vector<uint32_t>& task_ids, uint32_t dragged_id, uint32_t destination_id) {
  std::optional<std::size_t> dragged_position;
  std::optional<std::size_t> destination_position;

  for (std::size_t position = 0; position < task_ids.size(); ++position) {
    if (task_ids[position] == dragged_id) dragged_position = position;
    if (task_ids[position] == destination_id) destination_position = position;
  }

  if (!dragged_position || !destination_position) return std::nullopt;
  return std::pair{*dragged_position, *destination_position};
}

}  // namespace waybar::modules::wlr::dnd
