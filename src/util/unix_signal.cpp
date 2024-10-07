#include "util/unix_signal.hpp"

#include <spdlog/spdlog.h>
#include <sys/wait.h>

#include <stdexcept>

#include "util/format.hpp"

namespace waybar {

UnixSignalHandler::UnixSignalHandler() {
  sigemptyset(&mask_);
  /* Always handle SIGCHLD */
  sigaddset(&mask_, SIGCHLD);

  signal_.connect([this](int signum) {
    try {
      if (auto it = handlers_.find(signum); it != handlers_.end()) {
        it->second(signum);
      }
    } catch (const std::exception& e) {
      spdlog::error("Error handling signal {}: {}", signum, e.what());
    } catch (const Glib::Exception& e) {
      spdlog::error("Error handling signal {}: {}", signum, e.what());
    }
  });
}

void UnixSignalHandler::addChild(pid_t pid) {
  std::unique_lock lock{reap_mtx_};
  reap_.emplace_back(pid);
}

UnixSignalHandler::SignalProxy& UnixSignalHandler::signal(int signum) {
  if (thread_.joinable() && (sigismember(&mask_, signum) == 0)) {
    /*
     * At this point we may have more threads running with the original sigmask,
     * and it's too late to block new signals for them.
     */
    throw std::runtime_error("Cannot add signal to already running signal thread");
  }

  sigaddset(&mask_, signum);
  return handlers_[signum];
}

void UnixSignalHandler::start() {
  // Block signals so they can be handled by the signal thread
  // Any threads created by this one (the main thread) should not
  // modify their signal mask to unblock the handled signals
  if (int err = pthread_sigmask(SIG_BLOCK, &mask_, nullptr); err != 0) {
    spdlog::error("pthread_sigmask failed in UnixSignalHandler::start: {}", strerror(err));
    exit(1);
  }

  running_ = true;
  thread_ = std::thread([this]() { run(); });
}

UnixSignalHandler::~UnixSignalHandler() {
  running_ = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void UnixSignalHandler::run() {
  struct timespec timeout;
  timeout.tv_sec = 0;
  timeout.tv_nsec = 100 * 1000 * 1000;

  while (running_) {
    switch (int signum = sigtimedwait(&mask_, nullptr, &timeout)) {
      case -1:
        if (errno != EAGAIN && errno != EINTR) {
          spdlog::error("sigtimedwait failed: {}", strerror(errno));
        }
        break;

      case SIGCHLD: {
        spdlog::debug("Received SIGCHLD in signal thread");

        std::unique_lock lock{reap_mtx_};
        for (auto it = reap_.begin(); it != reap_.end();) {
          if (waitpid(*it, nullptr, WNOHANG) == *it) {
            spdlog::debug("Reaped child with PID: {}", *it);
            it = reap_.erase(it);
          } else {
            ++it;
          }
        }
        break;
      }
      default:
        signal_(signum);
        break;
    }
  }
}

}  // namespace waybar
