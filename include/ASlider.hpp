#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <regex>

#include "AModule.hpp"
#include "gtkmm/gesturedrag.h"
#include "gtkmm/scale.h"
#include "util/write_coalescer.hpp"

namespace waybar {

enum class WriteBehaviour : uint8_t {
  ON_RELEASE,  // commit once, on drag end (default)
  THROTTLED,   // rate-limited continuous commit
  DEBOUNCED,   // commit after movement stops
};

enum class FailureBehaviour : uint8_t {
  KEEP,  // retain the last value, mark stale, never write (default)
  MIN,   // snap the handle to min_
  HIDE,  // hide the widget until a read succeeds
};

class ASlider : public AModule {
 public:
  ASlider(const Json::Value& config, const std::string& name, const std::string& id,
          uint16_t interval = 0);

 protected:
  // user-initiated changes only. See handleChangeValue.
  virtual void onCommit(int value) = 0;
  void setValueSilently(int value);
  // True while a drag is in flight
  bool commitPending() const;
  // Mark the last read failed (or recovered): toggles .stale and applies
  // failure_behaviour_
  void setStale(bool stale);
  // Extract a value from tool output, clamped to [min_, max_]; nullopt on failure
  // Uses value-regex group 1 if set, else strips a trailing '%'.
  std::optional<double> parseValue(const std::string& raw) const;

  bool vertical_ = false;
  int min_ = 0, max_ = 100, curr_ = 50;
  // Equivalent to AGraph/ALabel. 0 is a sentinel, see ALabel #4521.
  const std::chrono::milliseconds interval_;
  // Guards against stray writes
  bool updating_ = false;
  WriteBehaviour write_behaviour_ = WriteBehaviour::ON_RELEASE;
  std::chrono::milliseconds write_interval_{40};
  FailureBehaviour failure_behaviour_ = FailureBehaviour::KEEP;
  bool stale_ = false;
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
  // Compiled once from value-regex, nullopt uses the default
  std::optional<std::regex> value_regex_;
  int pending_ = 0;
  bool has_pending_ = false;
  bool dragging_ = false;
};

}  // namespace waybar