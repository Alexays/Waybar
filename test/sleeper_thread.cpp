#include "util/sleeper_thread.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

TEST_CASE("Threaded execution", "[SleeperThread]") {
  waybar::util::CancellationGuard cancelGuard;
  bool functionWasCalled = false;
  std::thread::id callingThreadId = std::this_thread::get_id();
  auto mockFunction = [&functionWasCalled, &callingThreadId]() {
    functionWasCalled = true;
    callingThreadId = std::this_thread::get_id();
  };
  {
    waybar::util::SleeperThread testThread(mockFunction);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  CHECK(callingThreadId != std::this_thread::get_id());
  REQUIRE(functionWasCalled);
}

TEST_CASE("Sleep", "[SleeperThread]") {
  waybar::util::CancellationGuard cancelGuard;
  std::atomic<int> counter = 0;
  waybar::util::SleeperThread testThread([&counter, &testThread]() {
    testThread.sleep();
    ++counter;
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  CHECK(counter == 0);
  testThread.wake_up();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  REQUIRE(counter == 1);
}

TEST_CASE("Sleep_for", "[SleeperThread]") {
  waybar::util::CancellationGuard cancelGuard;
  std::atomic<int> counter = 0;
  waybar::util::SleeperThread testThread([&counter, &testThread]() {
    testThread.sleep_for(std::chrono::milliseconds(10));
    ++counter;
  });
  CHECK(counter == 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  REQUIRE(counter > 0);
}

TEST_CASE("Sleep_until", "[SleeperThread]") {
  waybar::util::CancellationGuard cancelGuard;
  std::atomic<int> counter = 0;
  waybar::util::SleeperThread testThread([&counter, &testThread]() {
    testThread.sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(10));
    ++counter;
  });
  CHECK(counter == 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  REQUIRE(counter > 0);
}
