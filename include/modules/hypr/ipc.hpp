#pragma once

#include <fmt/format.h>
#include <sigc++/sigc++.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>

#include "bar.hpp"
#include "client.hpp"

namespace waybar::modules::hypr {

    std::string makeRequest(std::string);

}  // namespace waybar::modules::hypr
