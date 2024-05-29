#pragma once
#include <glibmm/main.h>
/**
 * Minimal Glib application to be used for tests that require Glib main loop
 */
class GlibTestsFixture : public sigc::trackable {
 public:
  GlibTestsFixture() : main_loop_{Glib::MainLoop::create()} {}

  void setTimeout(int timeout) {
    Glib::signal_timeout().connect_once([]() { throw std::runtime_error("Test timed out"); },
                                        timeout);
  }

  void run(std::function<void()> fn) {
    Glib::signal_idle().connect_once(fn);
    main_loop_->run();
  }

  void quit() { main_loop_->quit(); }

 protected:
  Glib::RefPtr<Glib::MainLoop> main_loop_;
};
