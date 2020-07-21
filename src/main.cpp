#include <csignal>
#include <list>
#include <sys/types.h>
#include <sys/wait.h>
#include <spdlog/spdlog.h>
#include "client.hpp"

sig_atomic_t is_inserting_pid = false;
std::list<pid_t> reap;

static void handler(int sig) {
  int saved_errno = errno;
  if (!is_inserting_pid) {
    for (auto it = reap.begin(); it != reap.end(); ++it) {
      if (waitpid(*it, nullptr, WNOHANG) == *it) {
        it = reap.erase(it);
      }
    }
  }
  errno = saved_errno;
}

inline void installSigChldHandler(void) {
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = handler;
  sigaction(SIGCHLD, &sa, nullptr);
}

int main(int argc, char* argv[]) {
  try {
    auto client = waybar::Client::inst();
    std::signal(SIGUSR1, [](int /*signal*/) {
      for (auto& bar : waybar::Client::inst()->bars) {
        bar->toggle();
      }
    });

    for (int sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
      std::signal(sig, [](int sig) {
        for (auto& bar : waybar::Client::inst()->bars) {
          bar->handleSignal(sig);
        }
      });
    }
    installSigChldHandler();

    auto ret = client->main(argc, argv);
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
