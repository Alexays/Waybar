#include "util/SafeSignal.hpp"

#include <glibmm.h>

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif
#include <thread>
#include <type_traits>

#include "fixtures/GlibTestsFixture.hpp"

using namespace waybar;

template <typename T>
using remove_cvref_t = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

/**
 * Basic sanity test for SafeSignal:
 * check that type deduction works, events are delivered and the order is right
 * Running this with -fsanitize=thread should not fail
 */
TEST_CASE_METHOD(GlibTestsFixture, "SafeSignal basic functionality", "[signal][thread][util]") {
  const int NUM_EVENTS = 100;
  int count = 0;
  int last_value = 0;

  SafeSignal<int, std::string> test_signal;

  const auto main_tid = std::this_thread::get_id();
  std::thread producer;

  // timeout the test in 500ms
  setTimeout(500);

  test_signal.connect([&](auto val, auto str) {
    static_assert(std::is_same<int, decltype(val)>::value);
    static_assert(std::is_same<std::string, decltype(str)>::value);
    // check that we're in the same thread as the main loop
    REQUIRE(std::this_thread::get_id() == main_tid);
    // check event order
    REQUIRE(val == last_value + 1);

    last_value = val;
    if (++count >= NUM_EVENTS) {
      this->quit();
    };
  });

  run([&]() {
    // check that events from the same thread are delivered and processed synchronously
    test_signal.emit(1, "test");
    REQUIRE(count == 1);

    // start another thread and generate events
    producer = std::thread([&]() {
      for (auto i = 2; i <= NUM_EVENTS; ++i) {
        test_signal.emit(i, "test");
      }
    });
  });
  producer.join();
  REQUIRE(count == NUM_EVENTS);
}

template <typename T>
struct TestObject {
  T value;
  unsigned copied = 0;
  unsigned moved = 0;

  TestObject(const T& v) : value(v){};
  ~TestObject() = default;

  TestObject(const TestObject& other)
      : value(other.value), copied(other.copied + 1), moved(other.moved) {}

  TestObject(TestObject&& other) noexcept
      : value(std::move(other.value)),
        copied(std::exchange(other.copied, 0)),
        moved(std::exchange(other.moved, 0) + 1) {}

  TestObject& operator=(const TestObject& other) {
    value = other.value;
    copied = other.copied + 1;
    moved = other.moved;
    return *this;
  }

  TestObject& operator=(TestObject&& other) noexcept {
    value = std::move(other.value);
    copied = std::exchange(other.copied, 0);
    moved = std::exchange(other.moved, 0) + 1;
    return *this;
  }

  bool operator==(T other) const { return value == other; }
  operator T() const { return value; }
};

/*
 * Check the number of copies/moves performed on the object passed through SafeSignal
 */
TEST_CASE_METHOD(GlibTestsFixture, "SafeSignal copy/move counter", "[signal][thread][util]") {
  const int NUM_EVENTS = 3;
  int count = 0;

  SafeSignal<TestObject<int>> test_signal;

  std::thread producer;

  // timeout the test in 500ms
  setTimeout(500);

  test_signal.connect([&](auto& val) {
    static_assert(std::is_same<TestObject<int>, remove_cvref_t<decltype(val)>>::value);

    /* explicit move in the producer thread */
    REQUIRE(val.moved <= 1);
    /* copy within the SafeSignal queuing code */
    REQUIRE(val.copied <= 1);

    if (++count >= NUM_EVENTS) {
      this->quit();
    };
  });

  run([&]() {
    test_signal.emit(1);
    REQUIRE(count == 1);
    producer = std::thread([&]() {
      for (auto i = 2; i <= NUM_EVENTS; ++i) {
        TestObject<int> t{i};
        // check that signal.emit accepts moved objects
        test_signal.emit(std::move(t));
      }
    });
  });
  producer.join();
  REQUIRE(count == NUM_EVENTS);
}
