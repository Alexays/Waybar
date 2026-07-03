#include "modules/cava/cavaGLSL.hpp"

#include <spdlog/spdlog.h>

#include <fstream>

waybar::modules::cava::CavaGLSL::CavaGLSL(const std::string& id, const Json::Value& config)
    : AModule(config, "cavaGLSL", id, false, false),
      backend_{waybar::modules::cava::CavaBackend::inst(config)} {
  set_name(name_);
  if (config_["hide_on_silence"].isBool()) hide_on_silence_ = config_["hide_on_silence"].asBool();
  if (!id.empty()) {
    get_style_context()->add_class(id);
  }
  get_style_context()->add_class(MODULE_CLASS);

  set_use_es(true);
  //  set_auto_render(true);
  signal_realize().connect(sigc::mem_fun(*this, &CavaGLSL::onRealize));
  signal_render().connect(sigc::mem_fun(*this, &CavaGLSL::onRender), false);

  // Get parameters_config struct from the backend
  prm_ = *backend_->getPrm();

  // Set widget length
  int length{0};
  if (config_["min-length"].isUInt())
    length = config_["min-length"].asUInt();
  else if (config_["max-length"].isUInt())
    length = config_["max-length"].asUInt();
  else
    length = prm_.sdl_width;

  set_size_request(length, prm_.sdl_height);

  // Subscribe for changes
  backend_->signal_audio_raw_update().connect(sigc::mem_fun(*this, &CavaGLSL::onUpdate));
  // Subscribe for silence
  backend_->signal_silence().connect(sigc::mem_fun(*this, &CavaGLSL::onSilence));
  event_box_.add(*this);
}

auto waybar::modules::cava::CavaGLSL::onUpdate(const ::cava::audio_raw& input) -> void {
  Glib::signal_idle().connect_once([this, input]() {
    m_data_ = std::make_shared<::cava::audio_raw>(input);
    if (silence_) {
      get_style_context()->remove_class("silent");
      if (!get_style_context()->has_class("updated")) get_style_context()->add_class("updated");
      show();
      silence_ = false;
    }

    queue_render();
  });
}

auto waybar::modules::cava::CavaGLSL::onSilence() -> void {
  Glib::signal_idle().connect_once([this]() {
    if (!silence_) {
      if (get_style_context()->has_class("updated")) get_style_context()->remove_class("updated");

      if (hide_on_silence_) hide();
      silence_ = true;
      get_style_context()->add_class("silent");
      // Set clear color to black
      glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
      queue_render();
    }
  });
}

bool waybar::modules::cava::CavaGLSL::onRender(const Glib::RefPtr<Gdk::GLContext>& context) {
  if (!m_data_) return true;
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glUniform1i(glGetUniformLocation(shaderProgram_, "inputTexture"), 0);

  glUniform1fv(uniform_bars_, m_data_->number_of_bars, m_data_->bars_raw);
  glUniform1fv(uniform_previous_bars_, m_data_->number_of_bars, m_data_->previous_bars_raw);
  glUniform1i(uniform_bars_count_, m_data_->number_of_bars);
  ++frame_counter;
  glUniform1f(uniform_time_, (frame_counter / backend_->getFrameTimeMilsec().count()) / 1e3);

  //  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return true;
}

void waybar::modules::cava::CavaGLSL::onRealize() {
  make_current();
  initShaders();
  initGLSL();
  initSurface();
}

struct colors {
  uint16_t R;
  uint16_t G;
  uint16_t B;
};

static void parse_color(char* color_string, struct colors* color) {
  if (color_string[0] == '#') {
    sscanf(++color_string, "%02hx%02hx%02hx", &color->R, &color->G, &color->B);
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

  GLuint gVBO{0};
  glGenBuffers(1, &gVBO);
  glBindBuffer(GL_ARRAY_BUFFER, gVBO);
  glBufferData(GL_ARRAY_BUFFER, 2 * 4 * sizeof(GLfloat), vertexData, GL_STATIC_DRAW);

  GLuint gIBO{0};
  glGenBuffers(1, &gIBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(GLuint), indexData, GL_STATIC_DRAW);

  GLuint gVAO{0};
  glGenVertexArrays(1, &gVAO);
  glBindVertexArray(gVAO);
  glEnableVertexAttribArray(gVertexPos2DLocation);

  glBindBuffer(GL_ARRAY_BUFFER, gVBO);
  glVertexAttribPointer(gVertexPos2DLocation, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), nullptr);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIBO);

  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

  // Create a texture to attach the framebuffer
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, prm_.sdl_width, prm_.sdl_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);

  // Check is framebuffer is complete
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    spdlog::error("{0}. Framebuffer not complete", name_);
  }

  // Unbind the framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  uniform_bars_ = glGetUniformLocation(shaderProgram_, "bars");
  uniform_previous_bars_ = glGetUniformLocation(shaderProgram_, "previous_bars");
  uniform_bars_count_ = glGetUniformLocation(shaderProgram_, "bars_count");
  uniform_time_ = glGetUniformLocation(shaderProgram_, "shader_time");

  GLuint err{glGetError()};
  if (err != 0) {
    spdlog::error("{0}. Error on initGLSL: {1}", name_, err);
  }
}

void waybar::modules::cava::CavaGLSL::initSurface() {
  colors color = {0};
  GLint uniform_bg_col{glGetUniformLocation(shaderProgram_, "bg_color")};
  parse_color(prm_.bcolor, &color);
  glUniform3f(uniform_bg_col, (float)color.R / 255.0, (float)color.G / 255.0,
              (float)color.B / 255.0);
  GLint uniform_fg_col{glGetUniformLocation(shaderProgram_, "fg_color")};
  parse_color(prm_.color, &color);
  glUniform3f(uniform_fg_col, (float)color.R / 255.0, (float)color.G / 255.0,
              (float)color.B / 255.0);
  GLint uniform_res{glGetUniformLocation(shaderProgram_, "u_resolution")};
  glUniform3f(uniform_res, (float)prm_.sdl_width, (float)prm_.sdl_height, 0.0f);
  GLint uniform_bar_width{glGetUniformLocation(shaderProgram_, "bar_width")};
  glUniform1i(uniform_bar_width, prm_.bar_width);
  GLint uniform_bar_spacing{glGetUniformLocation(shaderProgram_, "bar_spacing")};
  glUniform1i(uniform_bar_spacing, prm_.bar_spacing);
  GLint uniform_gradient_count{glGetUniformLocation(shaderProgram_, "gradient_count")};
  glUniform1i(uniform_gradient_count, prm_.gradient_count);
  GLint uniform_gradient_colors{glGetUniformLocation(shaderProgram_, "gradient_colors")};
  GLfloat gradient_colors[8][3];
  for (int i{0}; i < prm_.gradient_count; ++i) {
    parse_color(prm_.gradient_colors[i], &color);
    gradient_colors[i][0] = (float)color.R / 255.0;
    gradient_colors[i][1] = (float)color.G / 255.0;
    gradient_colors[i][2] = (float)color.B / 255.0;
  }
  glUniform3fv(uniform_gradient_colors, 8, (const GLfloat*)gradient_colors);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLE_FAN, 4, GL_UNSIGNED_INT, nullptr);
}

void waybar::modules::cava::CavaGLSL::initShaders() {
  shaderProgram_ = glCreateProgram();

  GLuint vertexShader{loadShader(prm_.vertex_shader, GL_VERTEX_SHADER)};
  GLuint fragmentShader{loadShader(prm_.fragment_shader, GL_FRAGMENT_SHADER)};

  glAttachShader(shaderProgram_, vertexShader);
  glAttachShader(shaderProgram_, fragmentShader);

  glLinkProgram(shaderProgram_);

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  // Check for linking errors
  GLint success, len;
  glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &success);
  if (!success) {
    glGetProgramiv(shaderProgram_, GL_INFO_LOG_LENGTH, &len);
    GLchar* infoLog{(char*)'\0'};
    glGetProgramInfoLog(shaderProgram_, len, &len, infoLog);
    spdlog::error("{0}. Shader linking error: {1}", name_, infoLog);
  }

  glReleaseShaderCompiler();
  glUseProgram(shaderProgram_);
}

GLuint waybar::modules::cava::CavaGLSL::loadShader(const std::string& fileName, GLenum type) {
  spdlog::debug("{0}. loadShader: {1}", name_, fileName);

  // Read shader source code from the file
  std::ifstream shaderFile{fileName};

  if (!shaderFile.is_open()) {
    spdlog::error("{0}. Could not open shader file: {1}", name_, fileName);
  }

  std::ostringstream buffer;
  buffer << shaderFile.rdbuf();  // read file content into stringstream
  std::string str{buffer.str()};
  const char* shaderSource = str.c_str();
  shaderFile.close();

  GLuint shaderID{glCreateShader(type)};
  if (shaderID == 0) spdlog::error("{0}. Error creating shader type: {0}", type);
  glShaderSource(shaderID, 1, &shaderSource, nullptr);
  glCompileShader(shaderID);

  // Check for compilation errors
  GLint success, len;

  glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);

  if (!success) {
    glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &len);

    GLchar* infoLog{(char*)'\0'};
    glGetShaderInfoLog(shaderID, len, nullptr, infoLog);
    spdlog::error("{0}. Shader compilation error in {1}: {2}", name_, fileName, infoLog);
  }

  return shaderID;
}
