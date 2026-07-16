#pragma once

#include <epoxy/gl.h>
#include <gtkmm/glarea.h>
#include <map>
#include <string>
#include <array>

#include <sigc++/connection.h>

#include "AModule.hpp"
#include "cava_backend.hpp"

namespace waybar::modules::cava {

class CavaGLSL final : public AModule {
 public:
  CavaGLSL(const std::string&, const Json::Value&);
  ~CavaGLSL();
  auto doAction(const std::string& name) -> void override;

 private:
  using Action = void (CavaGLSL::*)();

  Gtk::GLArea gl_area_;
  std::shared_ptr<CavaBackend> backend_;
  // Cached config params (deep-copied strings to avoid dangling char* on backend reload)
  int sdl_width_{0};
  int sdl_height_{0};
  int bar_width_{0};
  int bar_spacing_{0};
  int gradient_count_{0};
  std::string vertex_shader_;
  std::string fragment_shader_;
  std::string bcolor_;
  std::string color_;
  std::array<std::string, 8> gradient_colors_;
  int frame_counter_{0};
  bool silence_{false};
  bool hide_on_silence_{false};
  bool mapped_{false};
  // Cava method
  void pauseResume();
  auto onUpdate(const CavaBackend::AudioRaw& input) -> void;
  auto onSilence() -> void;
  auto onBackendConfigChanged() -> void;
  void cacheConfigParams(const ::cava::config_params& src);
  // Member variable to store audio data
  CavaBackend::AudioRaw m_data_;
  GLuint shaderProgram_{0};
  // OpenGL variables
  GLuint fbo_{0};
  GLuint texture_{0};
  GLuint vbo_{0};
  GLuint ibo_{0};
  GLuint vao_{0};
  GLint uniform_bars_;
  GLint uniform_previous_bars_;
  GLint uniform_bars_count_;
  GLint uniform_time_;
  GLint uniform_input_texture_;
  // Methods
  void onRealize();
  bool onRender(const Glib::RefPtr<Gdk::GLContext>& context);

  void initShaders();
  void initSurface();
  void initGLSL();
  GLuint loadShader(const std::string& fileName, GLenum type);
  void cleanupGL();

  // ModuleActionMap
  static const std::map<std::string, Action> actionMap_;

  sigc::connection audio_raw_update_conn_;
  sigc::connection silence_conn_;
  sigc::connection config_changed_conn_;
};
}  // namespace waybar::modules::cava
