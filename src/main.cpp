#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <csignal>
#include <list>
#include <mutex>

#include "client.hpp"

std::mutex reap_mtx;
std::list<pid_t> reap;
volatile bool reload;

void* signalThread(void* args) {
  int err;
  int signum;
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);

  while (true) {
    err = sigwait(&mask, &signum);
    if (err != 0) {
      spdlog::error("sigwait failed: {}", strerror(errno));
      continue;
    }

    switch (signum) {
      case SIGCHLD:
        spdlog::debug("Received SIGCHLD in signalThread");
        if (!reap.empty()) {
          reap_mtx.lock();
          for (auto it = reap.begin(); it != reap.end(); ++it) {
            if (waitpid(*it, nullptr, WNOHANG) == *it) {
              spdlog::debug("Reaped child with PID: {}", *it);
              it = reap.erase(it);
            }
          }
          reap_mtx.unlock();
        }
        break;
      default:
        spdlog::debug("Received signal with number {}, but not handling", signum);
        break;
    }
  }
}

void startSignalThread() {
  int err;
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);

  // Block SIGCHLD so it can be handled by the signal thread
  // Any threads created by this one (the main thread) should not
  // modify their signal mask to unblock SIGCHLD
  err = pthread_sigmask(SIG_BLOCK, &mask, nullptr);
  if (err != 0) {
    spdlog::error("pthread_sigmask failed in startSignalThread: {}", strerror(err));
    exit(1);
  }

  pthread_t thread_id;
  err = pthread_create(&thread_id, nullptr, signalThread, nullptr);
  if (err != 0) {
    spdlog::error("pthread_create failed in startSignalThread: {}", strerror(err));
    exit(1);
  }
}

int main(int argc, char* argv[]) {
  try {
    auto* client = waybar::Client::inst();

    std::signal(SIGUSR1, [](int /*signal*/) {
      for (auto& bar : waybar::Client::inst()->bars) {
        bar->toggle();
      }
    });

    std::signal(SIGUSR2, [](int /*signal*/) {
      spdlog::info("Reloading...");
      reload = true;
      waybar::Client::inst()->reset();
    });

    std::signal(SIGINT, [](int /*signal*/) {
      spdlog::info("Quitting.");
      reload = false;
      waybar::Client::inst()->reset();
    });

    for (int sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
      std::signal(sig, [](int sig) {
        for (auto& bar : waybar::Client::inst()->bars) {
          bar->handleSignal(sig);
        }
      });
    }
    startSignalThread();

    auto ret = 0;
    do {
      reload = false;
      ret = client->main(argc, argv);
    } while (reload);

    delete client;
    return ret;
  } catch (const std::exception& e) {
    spdlog::error("{}", e.what());
    return 1;
  } catch (const Glib::Exception& e) {
    spdlog::error("{}", static_cast<std::string>(e.what()));
    return 1;
  }
}
