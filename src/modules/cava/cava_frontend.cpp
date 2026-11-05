#include "modules/cava/cavaRaw.hpp"
#include "modules/cava/cava_backend.hpp"
#ifdef HAVE_LIBCAVAGLSL
#include "modules/cava/cavaGLSL.hpp"
#endif

// Entry point looked up by Factory once waybar-cava.so is loaded.
extern "C" waybar::AModule* waybar_module_create(const std::string& id, const Json::Value& config) {
  using namespace waybar::modules::cava;
#ifdef HAVE_LIBCAVAGLSL
  if (CavaBackend::inst(config)->getPrm().output == ::cava::output_method::OUTPUT_SDL_GLSL) {
    return new CavaGLSL(id, config);
  }
#endif
  return new CavaRaw(id, config);
}
