#include "ASlider.hpp"

#include <algorithm>
#include <cmath>

#include "gtkmm/adjustment.h"
#include "gtkmm/enums.h"

namespace waybar {

ASlider::ASlider(const Json::Value& config, const std::string& name, const std::string& id,
                 uint16_t interval)
    : AModule(config, name, id, false, false),
      vertical_(config_["orientation"].asString() == "vertical"),
      interval_(
          config_["interval"] == "once"
              ? std::chrono::milliseconds::max()
              : std::chrono::milliseconds(
                    config_["interval"].isNumeric()
                        ? std::max(1L, static_cast<long>(config_["interval"].asDouble() * 1000))
                        : 1000L * static_cast<long>(interval))),
      scale_(vertical_ ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL) {
  scale_.set_name(name);
  if (!id.empty()) {
    scale_.get_style_context()->add_class(id);
  }
  scale_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(scale_);
  // change_value: user actions only. value_changed also fires on set_value (the write-back loop).
  scale_.signal_change_value().connect(sigc::mem_fun(*this, &ASlider::handleChangeValue));

  // A drag has no drag-end signal on Gtk::Range; observe it with a passive gesture.
  drag_gesture_ = Gtk::GestureDrag::create(scale_);
  drag_gesture_->signal_drag_begin().connect(sigc::mem_fun(*this, &ASlider::onDragBegin));
  drag_gesture_->signal_drag_end().connect(sigc::mem_fun(*this, &ASlider::onDragEnd));
  scale_.signal_grab_broken_event().connect(sigc::mem_fun(*this, &ASlider::onGrabBroken));

  if (config_["write-behaviour"].isString()) {
    const auto& behaviour = config_["write-behaviour"].asString();
    if (behaviour == "throttled") {
      write_behaviour_ = WriteBehaviour::THROTTLED;
    } else if (behaviour == "debounced") {
      write_behaviour_ = WriteBehaviour::DEBOUNCED;
    }
  }

  if (config_["min"].isUInt()) {
    min_ = config_["min"].asUInt();
  }

  if (config_["max"].isUInt()) {
    max_ = config_["max"].asUInt();
  }

  scale_.set_inverted(vertical_);
  scale_.set_draw_value(false);
  scale_.set_adjustment(Gtk::Adjustment::create(curr_, min_, max_ + 1, 1, 1, 1));
}

bool ASlider::handleChangeValue(Gtk::ScrollType scroll_type, double new_value) {
  // Belt-and-braces; change_value cannot fire for a programmatic set_value.
  if (updating_) {
    return false;
  }
  int value = std::clamp(static_cast<int>(std::lround(new_value)), min_, max_);
  // A drag streams SCROLL_JUMP; under ON_RELEASE hold it and commit on drag end.
  // Discrete keyboard/wheel steps are one intent each, so commit immediately.
  if (write_behaviour_ == WriteBehaviour::ON_RELEASE && dragging_ &&
      scroll_type == Gtk::SCROLL_JUMP) {
    pending_ = value;
    has_pending_ = true;
    return false;
  }
  onCommit(value);
  // false lets GTK still move the handle.
  return false;
}

void ASlider::onDragBegin(double /*start_x*/, double /*start_y*/) { dragging_ = true; }

void ASlider::onDragEnd(double /*offset_x*/, double /*offset_y*/) { commit(); }

bool ASlider::onGrabBroken(GdkEventGrabBroken* /*event*/) {
  // A broken grab yields no drag-end; flush so the held value is not lost.
  commit();
  return false;
}

void ASlider::commit() {
  dragging_ = false;
  if (has_pending_) {
    has_pending_ = false;
    onCommit(pending_);
  }
}

bool ASlider::commitPending() const { return dragging_; }

void ASlider::setValueSilently(int value) {
  updating_ = true;
  scale_.set_value(value);
  updating_ = false;
}

}  // namespace waybar