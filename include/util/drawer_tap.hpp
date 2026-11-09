#pragma once

namespace waybar::util {

enum class GroupPress { Pass, Open, Close };

struct GroupPressState {
  bool is_drawer;
  bool click_to_reveal;
  bool touch;
  bool primary_press;
  // For touch, whether the drawer is revealed; for the mouse, whether the group is prelit
  bool open;
  bool on_drawer_child;
};

// A touchscreen gives GTK no hover, so a tap on a drawer works like a click
// with click-to-reveal. A tap on a child of an open hover drawer passes
// through so the child's own click action runs without closing the drawer
inline GroupPress group_press(const GroupPressState& s) {
  if (!(s.click_to_reveal || (s.touch && s.is_drawer)) || !s.primary_press) {
    return GroupPress::Pass;
  }
  if (s.touch && !s.click_to_reveal && s.open && s.on_drawer_child) {
    return GroupPress::Pass;
  }
  return s.open ? GroupPress::Close : GroupPress::Open;
}

}  // namespace waybar::util
