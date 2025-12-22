#pragma once

#include <epoxy/gl.h>

#include "AModule.hpp"
#include "cava_backend.hpp"

namespace waybar::modules::cava {

class CavaGLSL final : public AModule, public Gtk::GLArea {
 public:
  CavaGLSL(const std::string&, const Json::Value&, std::mutex&, std::list<pid_t>&);
  ~CavaGLSL() = default;

 private:
  std::shared_ptr<CavaBackend> backend_;
  struct ::cava::config_params prm_;
  int frame_counter{0};
  bool silence_{false};
  bool hide_on_silence_{false};
  // Cava method
  auto onUpdate(const ::cava::audio_raw& input) -> void;
  auto onSilence() -> void;
  // Member variable to store the shared pointer
  std::shared_ptr<::cava::audio_raw> m_data_;
  GLuint shaderProgram_;
  // OpenGL variables
  GLuint fbo_;
  GLuint texture_;
  GLint uniform_bars_;
  GLint uniform_previous_bars_;
  GLint uniform_bars_count_;
  GLint uniform_time_;
  // Methods
  void onRealize();
  bool onRender(const Glib::RefPtr<Gdk::GLContext>& context);

  void initShaders();
  void initSurface();
  void initGLSL();
  GLuint loadShader(const std::string& fileName, GLenum type);
};
}  // namespace waybar::modules::cava
