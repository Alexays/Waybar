#include "util/sleeper_thread.hpp"

#include <condition_variable>
#include <mutex>

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
  { waybar::util::SleeperThread testThread(mockFunction); }
  CHECK(callingThreadId != std::this_thread::get_id());
  REQUIRE(functionWasCalled);
}

TEST_CASE("Sleep", "[SleeperThread]") {
  waybar::util::CancellationGuard cancelGuard;
  std::atomic<int> counter = 0;
  std::mutex waitMutex;
  std::unique_lock waitLock{waitMutex};
  std::condition_variable waitVar;
  waybar::util::SleeperThread testThread([&counter, &testThread, &waitVar]() {
    ++counter;
    waitVar.notify_all();
    testThread.sleep();
    ++counter;
    waitVar.notify_all();
  });
  waitVar.wait_for(waitLock, std::chrono::seconds{5}, [&counter]() { return counter > 0; });
  CHECK(counter >= 1);
  waitVar.wait_for(waitLock, std::chrono::seconds{5}, [&counter, &testThread]() {
    testThread.wake_up();
    return counter > 1;
  });
  REQUIRE(counter >= 2);
}

TEST_CASE("Sleep_for", "[SleeperThread]") {
  waybar::util::CancellationGuard cancelGuard;
  std::atomic<int> counter = 0;
  const auto start = std::chrono::steady_clock::now();
  {
    waybar::util::SleeperThread testThread([&counter, &testThread, start]() {
      testThread.sleep_for(std::chrono::milliseconds(10));
      counter = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
  }
  REQUIRE(counter > 10);
}

TEST_CASE("Sleep_until", "[SleeperThread]") {
  waybar::util::CancellationGuard cancelGuard;
  std::atomic<int> counter = 0;
  const auto start = std::chrono::steady_clock::now();
  waybar::util::SleeperThread testThread([&counter, &testThread, start]() {
    testThread.sleep_until(std::chrono::system_clock::now() + std::chrono::milliseconds(10));
    counter = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count();
  });
  CHECK(counter == 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  REQUIRE(counter > 10);
}
