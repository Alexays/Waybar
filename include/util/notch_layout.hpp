#pragma once

#include <algorithm>
#include <utility>
#include <vector>

namespace waybar::util {

struct NotchSlot {
  int x = 0;
  bool visible = false;
};

struct NotchLayout {
  std::vector<NotchSlot> slots;  // same order as the widths passed in
  // Occupied spans [start, end), one per side of the zone that has something on it.
  std::vector<std::pair<int, int>> segments;
};

// Places items (widths in visual left-to-right order) right to left from `right`, never letting one
// overlap [zone_start, zone_end): an item that would touch the zone goes to its left side and the
// flow continues from there. Items that would cross `left` are hidden, and so is everything before
// them, so the visible items keep their configured order.
inline NotchLayout notch_layout(const std::vector<int>& widths, int left, int right, int zone_start,
                                int zone_end, int spacing) {
  NotchLayout layout;
  layout.slots.resize(widths.size());
  const bool has_zone = zone_end > zone_start;

  int cursor = right;  // right edge of the next item
  int seg_end = right;
  bool open_segment = false;

  for (auto i = static_cast<int>(widths.size()) - 1; i >= 0; i--) {
    const int width = widths[i];
    int x = cursor - width;
    if (has_zone && x < zone_end && x + width > zone_start) {
      // Would touch the zone: close the segment on this side and continue left of the zone.
      if (open_segment) layout.segments.emplace_back(cursor + spacing, seg_end);
      cursor = std::min(cursor, zone_start);
      seg_end = cursor;
      open_segment = false;
      x = cursor - width;
    }
    if (x < left) break;  // this one and everything before it stay hidden
    layout.slots[i] = {.x = x, .visible = true};
    cursor = x - spacing;
    open_segment = true;
  }
  if (open_segment) layout.segments.emplace_back(cursor + spacing, seg_end);
  return layout;
}

}  // namespace waybar::util
