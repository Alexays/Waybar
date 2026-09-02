#pragma once
#include <glibmm/main.h>
/**
 * Minimal Glib application to be used for tests that require Glib main loop
 */
class GlibTestsFixture : public sigc::trackable {
 public:
  GlibTestsFixture() : main_loop_{Glib::MainLoop::create()} {}
  ~GlibTestsFixture() { timeout_.disconnect(); }

  void setTimeout(int timeout) {
    timeout_.disconnect();
    timeout_ = Glib::signal_timeout().connect(
        []() {
          throw std::runtime_error("Test timed out");
          return false;
        },
        timeout);
  }

  void run(std::function<void()> fn) {
    Glib::signal_idle().connect_once(fn);
    main_loop_->run();
  }

  void quit() { main_loop_->quit(); }

 protected:
  Glib::RefPtr<Glib::MainLoop> main_loop_;
  sigc::connection timeout_;
};
