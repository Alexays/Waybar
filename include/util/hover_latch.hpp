#pragma once

#include <functional>
#include <utility>

namespace waybar::util {

// Turns pointer crossings into one enter and one leave per hover, however many
// enter/leave pairs GTK sends while the pointer stays put
class HoverLatch {
 public:
  using Handler = std::function<void(bool entering)>;

  explicit HoverLatch(Handler handler) : handler_{std::move(handler)} {}
  HoverLatch(const HoverLatch&) = delete;
  HoverLatch& operator=(const HoverLatch&) = delete;

  void set(bool hovered) {
    if (hovered_ == hovered) {
      return;
    }
    hovered_ = hovered;
    handler_(hovered);
  }

 private:
  Handler handler_;
  bool hovered_ = false;
};

}  // namespace waybar::util
