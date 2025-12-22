#include "modules/cava/cavaRaw.hpp"
#include "modules/cava/cava_backend.hpp"
#ifdef HAVE_LIBCAVAGLSL
#include "modules/cava/cavaGLSL.hpp"
#endif

namespace waybar::modules::cava {
AModule* getModule(const std::string& id, const Json::Value& config, std::mutex& reap_mtx,
                   std::list<pid_t>& reap) {
  const std::shared_ptr<CavaBackend> backend_{waybar::modules::cava::CavaBackend::inst(config)};
  switch (backend_->getPrm()->output) {
    case ::cava::output_method::OUTPUT_RAW:
      return new waybar::modules::cava::Cava(id, config, reap_mtx, reap);
      break;
#ifdef HAVE_LIBCAVAGLSL
    case ::cava::output_method::OUTPUT_SDL_GLSL:
      return new waybar::modules::cava::CavaGLSL(id, config, reap_mtx, reap);
      break;
#endif
    default:
      break;
  }
  throw std::runtime_error("Unknown module");
};
}  // namespace waybar::modules::cava

extern "C" {
waybar::AModule* new_cava(const std::string& id, const Json::Value& config, std::mutex& reap_mtx,
                          std::list<pid_t>& reap) {
  return waybar::modules::cava::getModule(id, config, reap_mtx, reap);
}
}
