#include <spdlog/spdlog.h>

#include "client.hpp"
#include "util/unix_signal.hpp"

waybar::UnixSignalHandler signal_handler;

int main(int argc, char* argv[]) {
  try {
    auto* client = waybar::Client::inst();

    signal_handler.signal(SIGUSR1).connect(sigc::mem_fun(*client, &waybar::Client::toggle));
    signal_handler.signal(SIGUSR2).connect(sigc::mem_fun(*client, &waybar::Client::reload));
    signal_handler.signal(SIGINT).connect(sigc::mem_fun(*client, &waybar::Client::quit));
    for (int sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
      signal_handler.signal(sig).connect(sigc::mem_fun(*client, &waybar::Client::handleSignal));
    }
    signal_handler.start();

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
