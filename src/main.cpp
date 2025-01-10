#include <fcntl.h>
#include <spdlog/spdlog.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
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

static int signal_fds[2];

extern "C" void raw_signal_handler(int signum) {
  // just write a single byte to be more reliable
  auto sigbyte = static_cast<unsigned char>(signum);
  // The only way I know of that this could fail is if we have queued a truly
  // remarkable number of signals without handling them
  write(signal_fds[1], &sigbyte, 1);
}

static void dispatch_signal(unsigned char signum) {
  auto client = waybar::Client::inst();
  if (client == nullptr) {
    // TODO: should we do something for SIGINT?
    return;
  }
  if (signum == SIGUSR1) {
    for (auto& bar : client->bars) {
      bar->toggle();
    }
  } else if (signum == SIGUSR2) {
    spdlog::info("Reloading...");
    reload = true;
    client->reset();
  } else if (signum == SIGINT) {
    spdlog::info("Quitting.");
    reload = false;
    client->reset();
  } else if (signum > SIGRTMIN && signum <= SIGRTMAX) {
    for (auto& bar : client->bars) {
      bar->handleSignal(signum);
    }
  }
}

static bool handle_signals(Glib::IOCondition cond) {
  unsigned char buf[16];
  const size_t bufsize = sizeof(buf);
  while (true) {
    auto n = read(signal_fds[0], buf, bufsize);
    if (n < 0) {
      if (errno == EAGAIN) {
        return true;
      }
      throw std::system_error(errno, std::system_category());
    }
    for (int i = 0; i < n; i++) {
      dispatch_signal(buf[i]);
    }
    if (static_cast<size_t>(n) < bufsize) {
      return true;
    }
  }
}

int main(int argc, char* argv[]) {
  try {
    auto* client = waybar::Client::inst();

    // It would be nice if we could use g_unix_signal_add, but unfortunately, that
    // doesn't support RT signals
    if (pipe(signal_fds)) {
      throw std::runtime_error("Failed to create pipe");
    }
    std::signal(SIGUSR1, &raw_signal_handler);
    std::signal(SIGUSR2, &raw_signal_handler);
    std::signal(SIGINT, &raw_signal_handler);
    for (int sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
      std::signal(sig, &raw_signal_handler);
    }

    auto signalPipe = Glib::IOChannel::create_from_fd(signal_fds[0]);
    // Make the read side, non-blocking
    int pipe_flags = fcntl(signal_fds[0], F_GETFL);
    if (pipe_flags != -1) {
      pipe_flags = fcntl(signal_fds[0], F_SETFL, pipe_flags | O_NONBLOCK);
    }
    if (pipe_flags == -1) {
      throw std::runtime_error("Failed to set pipe to nonblocking mode");
    }

    Glib::signal_io().connect(sigc::ptr_fun(handle_signals), signal_fds[0],
                              Glib::IOCondition::IO_IN);

    startSignalThread();

    auto ret = 0;
    do {
      reload = false;
      ret = client->main(argc, argv);
    } while (reload);

    std::signal(SIGUSR1, SIG_IGN);
    std::signal(SIGUSR2, SIG_IGN);
    std::signal(SIGINT, SIG_IGN);

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
