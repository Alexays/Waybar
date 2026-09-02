#pragma once

#include <memory>

#ifdef HAVE_LIBCAVA
#include "cavaRaw.hpp"
#include "cava_backend.hpp"
#ifdef HAVE_LIBCAVAGLSL
#include "cavaGLSL.hpp"
#endif
#endif

namespace waybar::modules::cava {
inline std::unique_ptr<AModule> getModule(const std::string& id, const Json::Value& config) {
#ifdef HAVE_LIBCAVA
  const std::shared_ptr<CavaBackend> backend_{waybar::modules::cava::CavaBackend::inst(config)};
  switch (backend_->getPrm().output) {
#ifdef HAVE_LIBCAVAGLSL
    case ::cava::output_method::OUTPUT_SDL_GLSL:
      return std::make_unique<waybar::modules::cava::CavaGLSL>(id, config);
#endif
    default:
      return std::make_unique<waybar::modules::cava::CavaRaw>(id, config);
  }
#else
  throw std::runtime_error("Unknown module");
#endif
};
}  // namespace waybar::modules::cava
