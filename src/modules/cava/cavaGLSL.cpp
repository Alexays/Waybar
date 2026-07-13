#include "modules/cava/cavaGLSL.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

const std::map<std::string, waybar::modules::cava::CavaGLSL::Action>
    waybar::modules::cava::CavaGLSL::actionMap_{{"mode", &CavaGLSL::pauseResume}};

waybar::modules::cava::CavaGLSL::CavaGLSL(const std::string& id, const Json::Value& config)
    : AModule(config, "cavaGLSL", id, false, false),
      backend_{waybar::modules::cava::CavaBackend::inst(config)} {
  gl_area_.set_name(name_);
  if (config_["hide_on_silence"].isBool()) hide_on_silence_ = config_["hide_on_silence"].asBool();
  if (!id.empty()) {
    gl_area_.get_style_context()->add_class(id);
  }
  gl_area_.get_style_context()->add_class(MODULE_CLASS);

  gl_area_.set_use_es(true);
  gl_area_.signal_realize().connect(sigc::mem_fun(*this, &CavaGLSL::onRealize));
  gl_area_.signal_render().connect(sigc::mem_fun(*this, &CavaGLSL::onRender), false);
  gl_area_.signal_map().connect([this]() { mapped_ = true; });
  gl_area_.signal_unmap().connect([this]() { mapped_ = false; });

  cacheConfigParams(backend_->getPrm());

  int length{0};
  if (config_["min-length"].isUInt())
    length = config_["min-length"].asUInt();
  else if (config_["max-length"].isUInt())
    length = config_["max-length"].asUInt();
  else
    length = sdl_width_;

  gl_area_.set_size_request(length, sdl_height_);

  audio_raw_update_conn_ =
      backend_->signalAudioRawUpdate().connect(sigc::mem_fun(*this, &CavaGLSL::onUpdate));
  silence_conn_ = backend_->signalSilence().connect(sigc::mem_fun(*this, &CavaGLSL::onSilence));
  config_changed_conn_ =
      backend_->signalConfigChanged().connect(sigc::mem_fun(*this, &CavaGLSL::onBackendConfigChanged));
  event_box_.add(gl_area_);
}

waybar::modules::cava::CavaGLSL::~CavaGLSL() {
  audio_raw_update_conn_.disconnect();
  silence_conn_.disconnect();
  config_changed_conn_.disconnect();

  if (gl_area_.get_realized()) {
    gl_area_.make_current();
    cleanupGL();
  }
}

void waybar::modules::cava::CavaGLSL::cleanupGL() {
  if (shaderProgram_ != 0) {
    glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
  }
  if (fbo_ != 0) {
    glDeleteFramebuffers(1, &fbo_);
    fbo_ = 0;
  }
  if (texture_ != 0) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }
  if (vbo_ != 0) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (ibo_ != 0) {
    glDeleteBuffers(1, &ibo_);
    ibo_ = 0;
  }
  if (vao_ != 0) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
}

auto waybar::modules::cava::CavaGLSL::doAction(const std::string& name) -> void {
  auto it = actionMap_.find(name);
  if (it != actionMap_.end() && it->second) {
    (this->*it->second)();
  } else {
    spdlog::error("CavaGLSL. Unsupported action \"{0}\"", name);
  }
}

void waybar::modules::cava::CavaGLSL::pauseResume() { backend_->doPauseResume(); }

auto waybar::modules::cava::CavaGLSL::onUpdate(const CavaBackend::AudioRaw& input) -> void {
  m_data_ = input;
  if (silence_) {
    gl_area_.get_style_context()->remove_class("silent");
    if (!gl_area_.get_style_context()->has_class("updated"))
      gl_area_.get_style_context()->add_class("updated");
    gl_area_.show();
    silence_ = false;
  }

  if (mapped_) {
    gl_area_.queue_render();
  }
}

auto waybar::modules::cava::CavaGLSL::onSilence() -> void {
  if (!silence_) {
    if (gl_area_.get_style_context()->has_class("updated"))
      gl_area_.get_style_context()->remove_class("updated");

    if (hide_on_silence_) gl_area_.hide();
    silence_ = true;
    gl_area_.get_style_context()->add_class("silent");
  }
}

void waybar::modules::cava::CavaGLSL::cacheConfigParams(const ::cava::config_params& src) {
  sdl_width_ = src.sdl_width;
  sdl_height_ = src.sdl_height;
  bar_width_ = src.bar_width;
  bar_spacing_ = src.bar_spacing;
  gradient_count_ = src.gradient_count;
  vertex_shader_ = src.vertex_shader ? src.vertex_shader : "";
  fragment_shader_ = src.fragment_shader ? src.fragment_shader : "";
  bcolor_ = src.bcolor ? src.bcolor : "";
  color_ = src.color ? src.color : "";
  for (size_t i = 0; i < gradient_colors_.size(); ++i) {
    gradient_colors_[i] = (src.gradient_colors[i] ? src.gradient_colors[i] : "");
  }
}

auto waybar::modules::cava::CavaGLSL::onBackendConfigChanged() -> void {
  auto new_prm = backend_->getPrm();

  bool dimensions_changed =
      (new_prm.sdl_width != sdl_width_) || (new_prm.sdl_height != sdl_height_);

  bool shaders_changed = false;
  std::string new_vertex = new_prm.vertex_shader ? new_prm.vertex_shader : "";
  std::string new_fragment = new_prm.fragment_shader ? new_prm.fragment_shader : "";
  shaders_changed = (vertex_shader_ != new_vertex) || (fragment_shader_ != new_fragment);

  bool surface_changed = false;
  if (new_prm.bar_width != bar_width_) surface_changed = true;
  if (new_prm.bar_spacing != bar_spacing_) surface_changed = true;
  if (new_prm.gradient_count != gradient_count_) surface_changed = true;
  std::string new_bcolor = new_prm.bcolor ? new_prm.bcolor : "";
  if (bcolor_ != new_bcolor) surface_changed = true;
  std::string new_color = new_prm.color ? new_prm.color : "";
  if (color_ != new_color) surface_changed = true;
  for (size_t i = 0; i < gradient_colors_.size(); ++i) {
    std::string new_grad = new_prm.gradient_colors[i] ? new_prm.gradient_colors[i] : "";
    if (gradient_colors_[i] != new_grad) {
      surface_changed = true;
      break;
    }
  }

  cacheConfigParams(new_prm);

  if ((dimensions_changed || shaders_changed) && gl_area_.get_realized()) {
    gl_area_.make_current();
    cleanupGL();
    initShaders();
    if (shaderProgram_ != 0) {
      initGLSL();
      initSurface();
    }
  } else if (surface_changed && gl_area_.get_realized()) {
    gl_area_.make_current();
    glUseProgram(shaderProgram_);
    initSurface();
  }

  int length{0};
  if (config_["min-length"].isUInt())
    length = config_["min-length"].asUInt();
  else if (config_["max-length"].isUInt())
    length = config_["max-length"].asUInt();
  else
    length = sdl_width_;

  gl_area_.set_size_request(length, sdl_height_);
}

bool waybar::modules::cava::CavaGLSL::onRender(const Glib::RefPtr<Gdk::GLContext>& context) {
  if (m_data_.bars_raw.empty() || shaderProgram_ == 0) return true;

  glUseProgram(shaderProgram_);
  glBindVertexArray(vao_);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glUniform1i(uniform_input_texture_, 0);

  glUniform1fv(uniform_bars_, m_data_.number_of_bars, m_data_.bars_raw.data());
  glUniform1fv(uniform_previous_bars_, m_data_.number_of_bars, m_data_.previous_bars_raw.data());
  glUniform1i(uniform_bars_count_, m_data_.number_of_bars);
  ++frame_counter_;
  glUniform1f(uniform_time_,
              static_cast<float>(frame_counter_) * backend_->getFrameTimeMilsec().count() / 1000.0f);

  glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return true;
}

void waybar::modules::cava::CavaGLSL::onRealize() {
  gl_area_.make_current();
  cleanupGL();
  initShaders();
  if (shaderProgram_ == 0) {
    return;
  }
  initGLSL();
  initSurface();
}

struct Colors {
  uint16_t R;
  uint16_t G;
  uint16_t B;
};

static void parse_color(const char* color_string, struct Colors* color) {
  if (color_string == nullptr) {
    return;
  }
  if (color_string[0] != '#') {
    return;
  }
  if (std::strlen(color_string) < 7) {
    spdlog::warn("Invalid color string '{}': expected #RRGGBB", color_string);
    return;
  }
  if (std::sscanf(color_string + 1, "%02hx%02hx%02hx", &color->R, &color->G, &color->B) != 3) {
    spdlog::warn("Failed to parse color string '{}'", color_string);
  }
}

void waybar::modules::cava::CavaGLSL::initGLSL() {
  GLint gVertexPos2DLocation{glGetAttribLocation(shaderProgram_, "vertexPosition_modelspace")};
  if (gVertexPos2DLocation == -1) {
    spdlog::error("{0}. Could not find vertex position shader variable", name_);
  }

  glClearColor(0.f, 0.f, 0.f, 1.f);

  GLfloat vertexData[]{-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
  GLint indexData[]{0, 1, 2, 3};

  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(GLfloat), vertexData, GL_STATIC_DRAW);

  glGenBuffers(1, &ibo_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(GLuint), indexData, GL_STATIC_DRAW);

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);
  glEnableVertexAttribArray(gVertexPos2DLocation);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glVertexAttribPointer(gVertexPos2DLocation, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);

  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sdl_width_, sdl_height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("{0}. Framebuffer not complete", name_);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  uniform_bars_ = glGetUniformLocation(shaderProgram_, "bars");
  uniform_previous_bars_ = glGetUniformLocation(shaderProgram_, "previous_bars");
  uniform_bars_count_ = glGetUniformLocation(shaderProgram_, "bars_count");
  uniform_time_ = glGetUniformLocation(shaderProgram_, "shader_time");
  uniform_input_texture_ = glGetUniformLocation(shaderProgram_, "inputTexture");

  GLuint err{glGetError()};
  if (err != 0) {
    spdlog::error("{0}. Error on initGLSL: {1}", name_, err);
  }
}

void waybar::modules::cava::CavaGLSL::initSurface() {
  Colors color = {0};
  GLint uniform_bg_col{glGetUniformLocation(shaderProgram_, "bg_color")};
  parse_color(bcolor_.c_str(), &color);
  glUniform3f(uniform_bg_col, static_cast<float>(color.R) / 255.0f, static_cast<float>(color.G) / 255.0f,
              static_cast<float>(color.B) / 255.0f);
  GLint uniform_fg_col{glGetUniformLocation(shaderProgram_, "fg_color")};
  parse_color(color_.c_str(), &color);
  glUniform3f(uniform_fg_col, static_cast<float>(color.R) / 255.0f, static_cast<float>(color.G) / 255.0f,
              static_cast<float>(color.B) / 255.0f);
  GLint uniform_res{glGetUniformLocation(shaderProgram_, "u_resolution")};
  glUniform3f(uniform_res, static_cast<float>(sdl_width_), static_cast<float>(sdl_height_), 0.0f);
  GLint uniform_bar_width{glGetUniformLocation(shaderProgram_, "bar_width")};
  glUniform1i(uniform_bar_width, bar_width_);
  GLint uniform_bar_spacing{glGetUniformLocation(shaderProgram_, "bar_spacing")};
  glUniform1i(uniform_bar_spacing, bar_spacing_);
  GLint uniform_gradient_count{glGetUniformLocation(shaderProgram_, "gradient_count")};
  glUniform1i(uniform_gradient_count, std::max(0, gradient_count_));
  GLint uniform_gradient_colors{glGetUniformLocation(shaderProgram_, "gradient_colors")};
  GLfloat gradient_colors[8][3] = {};
  for (int i{0}; i < gradient_count_; ++i) {
    parse_color(gradient_colors_[i].c_str(), &color);
    gradient_colors[i][0] = static_cast<float>(color.R) / 255.0f;
    gradient_colors[i][1] = static_cast<float>(color.G) / 255.0f;
    gradient_colors[i][2] = static_cast<float>(color.B) / 255.0f;
  }
  glUniform3fv(uniform_gradient_colors, std::max(0, std::min(gradient_count_, 8)), &gradient_colors[0][0]);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);
}

void waybar::modules::cava::CavaGLSL::initShaders() {
  shaderProgram_ = glCreateProgram();
  if (shaderProgram_ == 0) {
    spdlog::error("{0}. Failed to create shader program", name_);
    gl_area_.hide();
    return;
  }

  GLuint vertexShader{loadShader(vertex_shader_, GL_VERTEX_SHADER)};
  GLuint fragmentShader{loadShader(fragment_shader_, GL_FRAGMENT_SHADER)};

  if (vertexShader == 0 || fragmentShader == 0) {
    if (vertexShader != 0) glDeleteShader(vertexShader);
    if (fragmentShader != 0) glDeleteShader(fragmentShader);
    glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
    gl_area_.hide();
    return;
  }

  glAttachShader(shaderProgram_, vertexShader);
  glAttachShader(shaderProgram_, fragmentShader);

  glLinkProgram(shaderProgram_);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  GLint success{0};
  glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &success);
  if (!success) {
    GLint len{0};
    glGetProgramiv(shaderProgram_, GL_INFO_LOG_LENGTH, &len);
    std::vector<GLchar> infoLog(len + 1);
    glGetProgramInfoLog(shaderProgram_, len, nullptr, infoLog.data());
    spdlog::error("{0}. Shader linking error: {1}", name_, infoLog.data());
    glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
    gl_area_.hide();
    return;
  }

  glReleaseShaderCompiler();
  glUseProgram(shaderProgram_);
}

GLuint waybar::modules::cava::CavaGLSL::loadShader(const std::string& fileName, GLenum type) {
  spdlog::debug("{0}. loadShader: {1}", name_, fileName);

  std::ifstream shaderFile{fileName};
  if (!shaderFile.is_open()) {
    spdlog::error("{0}. Could not open shader file: {1}", name_, fileName);
    return 0;
  }

  std::ostringstream buffer;
  buffer << shaderFile.rdbuf();
  std::string str{buffer.str()};
  shaderFile.close();

  GLuint shaderID{glCreateShader(type)};
  if (shaderID == 0) {
    spdlog::error("{0}. Error creating shader type: {1}", name_, type);
    return 0;
  }

  const char* shaderSource = str.c_str();
  glShaderSource(shaderID, 1, &shaderSource, nullptr);
  glCompileShader(shaderID);

  GLint success{0};
  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
  if (!success) {
    GLint len{0};
    glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &len);
    std::vector<GLchar> infoLog(len + 1);
    glGetShaderInfoLog(shaderID, len, nullptr, infoLog.data());
    spdlog::error("{0}. Shader compilation error in {1}: {2}", name_, fileName, infoLog.data());
    glDeleteShader(shaderID);
    return 0;
  }

  return shaderID;
}
