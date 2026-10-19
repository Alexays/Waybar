#include "util/write_coalescer.hpp"

#include <glibmm/main.h>

#include <algorithm>

namespace waybar::util {

WriteCoalescer::WriteCoalescer(std::function<void(int)> writer, std::chrono::milliseconds min_gap,
                               CoalesceMode mode)
    : writer_(std::move(writer)), min_gap_(min_gap), mode_(mode) {}

WriteCoalescer::~WriteCoalescer() { timer_.disconnect(); }

void WriteCoalescer::submit(int value) {
  auto now = now_();
  pending_ = value;
  if (mode_ == CoalesceMode::THROTTLE) {
    // Leading edge: write at once if idle or a full gap has elapsed.
    if (!have_written_ || now - last_write_time_ >= min_gap_) {
      writeNow(value, now);
      return;
    }
    deadline_ = last_write_time_ + min_gap_;
  } else {
    // DEBOUNCE re-arms on every submit, so writes land only after movement stops.
    deadline_ = now + min_gap_;
  }
  arm();
}

void WriteCoalescer::flush() {
  timer_.disconnect();
  if (pending_) {
    writeNow(*pending_, now_());
  }
}

void WriteCoalescer::tick() {
  timer_.disconnect();
  if (!pending_ || !deadline_) {
    return;
  }
  auto now = now_();
  if (now < *deadline_) {
    arm();  // woke early; wait out the remainder
    return;
  }
  // Convergence: write the newest value if it differs from what landed last.
  if (!have_written_ || *pending_ != last_written_) {
    writeNow(*pending_, now);
  } else {
    pending_.reset();
    deadline_.reset();
  }
}

void WriteCoalescer::writeNow(int value, Clock::time_point now) {
  writer_(value);
  have_written_ = true;
  last_written_ = value;
  last_write_time_ = now;
  pending_.reset();
  deadline_.reset();
}

void WriteCoalescer::arm() {
  timer_.disconnect();
  if (!deadline_) {
    return;
  }
  auto now = now_();
  auto delay = *deadline_ > now
                   ? std::chrono::duration_cast<std::chrono::milliseconds>(*deadline_ - now)
                   : std::chrono::milliseconds(0);
  timer_ = Glib::signal_timeout().connect(
      [this] {
        tick();
        return false;
      },
      static_cast<unsigned>(std::max(0L, static_cast<long>(delay.count()))));
}

}  // namespace waybar::util
