#pragma once

#include <algorithm>

namespace waybar::util {

// Sizes and the center offset along the bar's main axis, measured from its start.
struct CenterLayout {
  int start_size;
  int center_pos;
  int center_size;
  int end_size;
};

/*
 * Keep the center block in the middle of `size` while both sides fit next to it, and move it
 * off-center only as far as the wider side needs. GtkBox instead caps each side at half of the
 * space around its center widget (gtk_box_size_allocate_with_center), which ellipsizes a wide
 * side even when the other side is nearly empty.
 */
inline CenterLayout center_layout(int size, int start_min, int start_nat, int center_min,
                                  int center_nat, int end_min, int end_nat) {
  int center = std::max(std::min(center_nat, size - start_min - end_min), center_min);
  int rest = size - center;
  // Natural sizes while they fit. When both sides overflow, split the rest evenly like GtkBox,
  // but never take what the end side needs as its minimum.
  int start = std::max(std::min({start_nat, std::max(rest / 2, rest - end_nat), rest - end_min}),
                       start_min);
  int end = std::max(std::min(end_nat, rest - start), end_min);
  int pos = std::max(start, std::min(rest / 2, size - end - center));
  return {start, pos, center, end};
}

}  // namespace waybar::util
