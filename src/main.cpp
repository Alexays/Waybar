#include <csignal>
#include <iostream>
#include "client.hpp"

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

    auto ret = client->main(argc, argv);
    delete client;
    return ret;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  } catch (const Glib::Exception& e) {
    std::cerr << e.what().c_str() << std::endl;
    return 1;
  }
}
