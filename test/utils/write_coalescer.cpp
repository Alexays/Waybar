#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <chrono>
#include <vector>

#include "util/write_coalescer.hpp"

namespace {

using waybar::util::CoalesceMode;
using waybar::util::WriteCoalescer;
using Clock = WriteCoalescer::Clock;
using std::chrono::milliseconds;

// A coalescer wired to a manual clock and a write log, for deterministic tests.
struct Harness {
  std::vector<int> writes;
  Clock::time_point now{};
  WriteCoalescer coalescer;

  explicit Harness(CoalesceMode mode, milliseconds gap = milliseconds(40))
      : coalescer([this](int v) { writes.push_back(v); }, gap, mode) {
    coalescer.setClock([this] { return now; });
  }
  void advance(milliseconds d) { now += d; }
};

}  // namespace

TEST_CASE("WriteCoalescer THROTTLE writes the first value at once (leading edge)",
          "[write_coalescer]") {
  Harness h(CoalesceMode::THROTTLE);
  h.coalescer.submit(10);
  REQUIRE(h.writes == std::vector<int>{10});
}

TEST_CASE("WriteCoalescer THROTTLE coalesces mid-gap submits to the newest", "[write_coalescer]") {
  Harness h(CoalesceMode::THROTTLE);
  h.coalescer.submit(10);  // leading write
  h.advance(milliseconds(10));
  h.coalescer.submit(20);  // within the gap: held
  h.advance(milliseconds(10));
  h.coalescer.submit(30);  // within the gap: overwrites 20
  REQUIRE(h.writes == std::vector<int>{10});
  h.advance(milliseconds(20));  // reach the deadline
  h.coalescer.tick();
  REQUIRE(h.writes == std::vector<int>{10, 30});  // 20 was never written
}

TEST_CASE("WriteCoalescer THROTTLE tick before the deadline does nothing", "[write_coalescer]") {
  Harness h(CoalesceMode::THROTTLE);
  h.coalescer.submit(10);
  h.advance(milliseconds(10));
  h.coalescer.submit(20);
  h.advance(milliseconds(10));  // now=20, deadline=40
  h.coalescer.tick();
  REQUIRE(h.writes == std::vector<int>{10});
}

TEST_CASE("WriteCoalescer flush writes the pending value immediately (convergence)",
          "[write_coalescer]") {
  Harness h(CoalesceMode::THROTTLE);
  h.coalescer.submit(10);
  h.advance(milliseconds(5));
  h.coalescer.submit(50);  // held
  h.coalescer.flush();
  REQUIRE(h.writes == std::vector<int>{10, 50});
}

TEST_CASE("WriteCoalescer DEBOUNCE writes only after silence, re-arming each submit",
          "[write_coalescer]") {
  Harness h(CoalesceMode::DEBOUNCE);
  h.coalescer.submit(10);  // no leading write
  REQUIRE(h.writes.empty());
  h.advance(milliseconds(20));
  h.coalescer.submit(20);  // re-arms: deadline now 60
  h.advance(milliseconds(20));
  h.coalescer.tick();  // now=40 < 60
  REQUIRE(h.writes.empty());
  h.advance(milliseconds(20));
  h.coalescer.tick();  // now=60
  REQUIRE(h.writes == std::vector<int>{20});
}

TEST_CASE("WriteCoalescer DEBOUNCE flush writes immediately", "[write_coalescer]") {
  Harness h(CoalesceMode::DEBOUNCE);
  h.coalescer.submit(10);
  h.coalescer.flush();
  REQUIRE(h.writes == std::vector<int>{10});
}

TEST_CASE("WriteCoalescer THROTTLE bounds writes over a burst and converges to the last",
          "[write_coalescer]") {
  Harness h(CoalesceMode::THROTTLE);  // 40 ms gap
  constexpr int kEvents = 100;
  for (int i = 1; i <= kEvents; ++i) {
    h.coalescer.submit(i);
    h.advance(milliseconds(5));  // ~500 ms of drag at 5 ms/event
    h.coalescer.tick();          // as if the timer fired whenever due
  }
  h.coalescer.flush();
  // 500 ms / 40 ms gap plus leading and flush: far fewer than kEvents.
  REQUIRE(h.writes.size() < 20);
  REQUIRE(h.writes.back() == kEvents);  // the final position is never dropped
}
