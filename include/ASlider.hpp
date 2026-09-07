#pragma once

#include <chrono>
#include <memory>

#include "AModule.hpp"
#include "gtkmm/gesturedrag.h"
#include "gtkmm/scale.h"
#include "util/write_coalescer.hpp"

namespace waybar {

enum class WriteBehaviour {
  ON_RELEASE,  // commit once, on drag end (default)
  THROTTLED,   // rate-limited continuous commit
  DEBOUNCED,   // commit after movement stops
};

class ASlider : public AModule {
 public:
  ASlider(const Json::Value& config, const std::string& name, const std::string& id,
          uint16_t interval = 0);

 protected:
  // Only write hook; user-initiated changes only. See handleChangeValue.
  virtual void onCommit(int value) = 0;
  // Programmatic display update; does not emit onCommit.
  void setValueSilently(int value);
  // True while a drag is in flight; a poll should not snap the handle away.
  bool commitPending() const;

  bool vertical_ = false;
  int min_ = 0, max_ = 100, curr_ = 50;
  // Equivalent to AGraph/ALabel. 0 is a sentinel, see ALabel #4521.
  const std::chrono::milliseconds interval_;
  // Guards against a stray write while we drive the scale ourselves.
  bool updating_ = false;
  WriteBehaviour write_behaviour_ = WriteBehaviour::ON_RELEASE;
  std::chrono::milliseconds write_interval_{40};
  Gtk::Scale scale_;

 private:
  bool handleChangeValue(Gtk::ScrollType scroll_type, double new_value);
  void onDragBegin(double start_x, double start_y);
  void onDragEnd(double offset_x, double offset_y);
  bool onGrabBroken(GdkEventGrabBroken* event);
  // Flush a held drag value and clear the drag state.
  void commit();

  Glib::RefPtr<Gtk::GestureDrag> drag_gesture_;
  // Only for THROTTLED/DEBOUNCED; null under ON_RELEASE.
  std::unique_ptr<util::WriteCoalescer> coalescer_;
  int pending_ = 0;
  bool has_pending_ = false;
  bool dragging_ = false;
};

}  // namespace waybar