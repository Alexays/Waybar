#include <spdlog/spdlog.h>
#include <csignal>
#include "client.hpp"

int main(int argc, char* argv[]) {
  try {
    auto client = waybar::Client::inst();
    std::signal(SIGUSR1, [](int /*signal*/) {
      Glib::signal_idle().connect_once([] {
        for (auto& bar : waybar::Client::inst()->bars) {
          bar->toggle();
        }
      });
    });

    for (int sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
      std::signal(sig, [](int sig) {
        for (auto& bar : waybar::Client::inst()->bars) {
          bar->handleSignal(sig);
        }
      });
    }

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
