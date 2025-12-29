#pragma once

#ifdef HAVE_LIBCAVA
#include "cavaRaw.hpp"
#include "cava_backend.hpp"
#ifdef HAVE_LIBCAVAGLSL
#include "cavaGLSL.hpp"
#endif
#endif

namespace waybar::modules::cava {
AModule* getModule(const std::string& id, const Json::Value& config, std::mutex& reap_mtx,
                   std::list<pid_t>& reap) {
#ifdef HAVE_LIBCAVA
  const std::shared_ptr<CavaBackend> backend_{waybar::modules::cava::CavaBackend::inst(config)};
  switch (backend_->getPrm()->output) {
#ifdef HAVE_LIBCAVAGLSL
    case ::cava::output_method::OUTPUT_SDL_GLSL:
      return new waybar::modules::cava::CavaGLSL(id, config, reap_mtx, reap);
#endif
    default:
      return new waybar::modules::cava::Cava(id, config, reap_mtx, reap);
  }
#else
  throw std::runtime_error("Unknown module");
#endif
};
}  // namespace waybar::modules::cava
