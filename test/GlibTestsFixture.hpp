#pragma once
#include <glibmm/main.h>
/**
 * Minimal Glib application to be used for tests that require Glib main loop
 */
class GlibTestsFixture : public sigc::trackable {
 public:
  GlibTestsFixture() : main_loop_{Glib::MainLoop::create()} {}

  void run(std::function<void()> fn) {
    Glib::signal_idle().connect_once(fn);
    main_loop_->run();
  }

  void quit() { main_loop_->quit(); }

 protected:
  Glib::RefPtr<Glib::MainLoop> main_loop_;
};
