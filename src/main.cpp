#include <spdlog/spdlog.h>

#include "client.hpp"
#include "util/unix_signal.hpp"

volatile bool reload;
waybar::UnixSignalHandler signal_handler;

int main(int argc, char* argv[]) {
  try {
    auto* client = waybar::Client::inst();

    signal_handler.signal(SIGUSR1).connect([](int /*signal*/) {
      for (auto& bar : waybar::Client::inst()->bars) {
        bar->toggle();
      }
    });

    signal_handler.signal(SIGUSR2).connect([](int /*signal*/) {
      spdlog::info("Reloading...");
      reload = true;
      waybar::Client::inst()->reset();
    });

    signal_handler.signal(SIGINT).connect([](int /*signal*/) {
      spdlog::info("Quitting.");
      reload = false;
      waybar::Client::inst()->reset();
    });

    for (int sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
      signal_handler.signal(sig).connect([](int sig) {
        for (auto& bar : waybar::Client::inst()->bars) {
          bar->handleSignal(sig);
        }
      });
    }
    signal_handler.start();

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
