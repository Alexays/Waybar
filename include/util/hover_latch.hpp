#pragma once

#include <glibmm/main.h>

#include <functional>
#include <utility>

namespace waybar::util {

// Turns pointer crossings into one enter and one leave per hover, however many
// enter/leave pairs GTK sends while the pointer stays put. With a delay, enter
// waits until the pointer has stayed that long, and a hover that ends sooner
// runs neither, so the two still come in pairs
class HoverLatch {
 public:
  using Handler = std::function<void(bool entering)>;

  HoverLatch(int delay_ms, Handler handler) : delay_ms_{delay_ms}, handler_{std::move(handler)} {}
  ~HoverLatch() { timeout_.disconnect(); }
  HoverLatch(const HoverLatch&) = delete;
  HoverLatch& operator=(const HoverLatch&) = delete;

  void set(bool hovered) {
    if (hovered_ == hovered) {
      return;
    }
    hovered_ = hovered;

    if (hovered && delay_ms_ > 0) {
      timeout_ = Glib::signal_timeout().connect(
          [this] {
            handler_(true);
            return false;
          },
          delay_ms_);
      return;
    }
    if (!hovered && timeout_.connected()) {
      timeout_.disconnect();
      return;
    }
    handler_(hovered);
  }

  // For a widget taken away from under the pointer, which gets no leave event
  void reset() {
    timeout_.disconnect();
    hovered_ = false;
  }

 private:
  const int delay_ms_;
  Handler handler_;
  bool hovered_ = false;
  sigc::connection timeout_;
};

}  // namespace waybar::util
