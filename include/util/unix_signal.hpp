#pragma once

#include <csignal>
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include "util/SafeSignal.hpp"

namespace waybar {

class UnixSignalHandler {
 public:
  class SignalProxy : private sigc::signal<void(int)> {
   public:
    friend class UnixSignalHandler;

    using sigc::signal<void(int)>::connect;
  };

  UnixSignalHandler();
  ~UnixSignalHandler();

  void addChild(pid_t pid);
  SignalProxy& signal(int signum);

  void start();

 private:
  void run();

  sigset_t mask_;
  std::thread thread_;
  std::mutex reap_mtx_;
  std::list<pid_t> reap_;
  std::atomic<bool> running_;

  SafeSignal<int> signal_;
  std::map<int, SignalProxy> handlers_;
};

}  // namespace waybar

extern waybar::UnixSignalHandler signal_handler;
