#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace waybar::modules::wlr::dnd {

inline std::optional<uint32_t> decode_task_id(const unsigned char* data, std::size_t length) {
  if (data == nullptr || length != sizeof(uint32_t)) {
    return std::nullopt;
  }

  uint32_t id;
  std::memcpy(&id, data, sizeof(id));
  return id;
}

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
