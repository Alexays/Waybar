#include "client.hpp"
#include <csignal>
#include <iostream>

namespace waybar {

static Client* client;

} // namespace waybar

int main(int argc, char* argv[])
{
  try {
    waybar::Client c(argc, argv);
    waybar::client = &c;
    std::signal(SIGUSR1, [] (int /*signal*/) {
      for (auto& bar : waybar::client->bars) {
        bar->toggle();
      }
    });

    return c.main(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  } catch (const Glib::Exception& e) {
    std::cerr << e.what().c_str() << std::endl;
    return 1;
  }
}
