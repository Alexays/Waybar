#include "modules/cava/cava_backend.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>

namespace {
struct CavaConfigGuard {
  ::cava::config_params* prm;
  bool released = false;
  explicit CavaConfigGuard(::cava::config_params* p) : prm(p) {}
  ~CavaConfigGuard() {
    if (!released && prm) {
      free_config(prm);
    }
  }
  void release() { released = true; }
};
}  // namespace

std::shared_ptr<waybar::modules::cava::CavaBackend> waybar::modules::cava::CavaBackend::inst(
    const Json::Value& config) {
  static auto* backend = new CavaBackend(config);
  static std::shared_ptr<CavaBackend> backend_ptr{backend};
  return backend_ptr;
}

waybar::modules::cava::CavaBackend::CavaBackend(const Json::Value& config) : config_(config) {
  loadConfig();
  read_thread_ = [this] {
    while (read_thread_.isRunning()) {
      try {
        if (input_source_) {
          input_source_(&audio_data_);
        }
      } catch (const std::exception& e) {
        spdlog::warn("Cava backend. Read source error: {0}", e.what());
      }
      if (!read_thread_.isRunning()) break;
      read_thread_.sleep_for(fetch_input_delay_);
      if (!read_thread_.isRunning()) break;
      try {
        loadConfig();
      } catch (const std::exception& e) {
        spdlog::error("{}", e.what());
      }
    }
    {
      std::lock_guard<std::mutex> lk(read_thread_exit_mutex_);
      read_thread_exited_ = true;
    }
    read_thread_exit_cv_.notify_one();
  };
  out_thread_ = [this] {
    try {
      doUpdate(false);
    } catch (const std::exception& e) {
      spdlog::error("Cava backend. Output thread error: {0}", e.what());
    }
    out_thread_.sleep_for(adaptive_delay_.current());
  };
}

waybar::modules::cava::CavaBackend::~CavaBackend() {
  {
    std::lock_guard<std::recursive_mutex> lock(state_mutex_);
    shutdown_ = true;
  }

  pthread_mutex_lock(&audio_data_.lock);
  audio_data_.terminate = 1;
  pthread_mutex_unlock(&audio_data_.lock);

  out_thread_.stop();
  read_thread_.stop();

  std::unique_lock<std::mutex> lk(read_thread_exit_mutex_);
  if (!read_thread_exit_cv_.wait_for(lk, std::chrono::milliseconds(100),
                                     [this] { return read_thread_exited_; })) {
    spdlog::debug(
        "Cava backend: read thread still blocked in input_source() on shutdown. "
        "Proceeding with cleanup.");
  }

  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  freeBackend();
}

bool waybar::modules::cava::CavaBackend::isSilent() {
  pthread_mutex_lock(&audio_data_.lock);
  bool silent = true;
  for (int i{0}; i < audio_data_.input_buffer_size; ++i) {
    if (audio_data_.cava_in[i]) {
      silent = false;
      break;
    }
  }
  pthread_mutex_unlock(&audio_data_.lock);
  return silent;
}

int waybar::modules::cava::CavaBackend::getAsciiRange() const {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  return prm_.ascii_range;
}

void waybar::modules::cava::CavaBackend::invoke() {
  pthread_mutex_lock(&audio_data_.lock);
  ::cava::cava_execute(audio_data_.cava_in, audio_data_.samples_counter, audio_raw_.cava_out,
                       plan_);
  if (audio_data_.samples_counter > 0) audio_data_.samples_counter = 0;
  pthread_mutex_unlock(&audio_data_.lock);
}

void waybar::modules::cava::CavaBackend::execute() {
  invoke();
  audio_raw_fetch(&audio_raw_, &prm_, &re_paint_, plan_);

  if (re_paint_ == 1) {
    output_.clear();
    for (int i{0}; i < audio_raw_.number_of_bars; ++i) {
      audio_raw_.previous_frame[i] = audio_raw_.bars[i];
      output_.push_back(audio_raw_.bars[i]);
      if (prm_.bar_delim != 0) output_.push_back(prm_.bar_delim);
    }
  }
}

void waybar::modules::cava::CavaBackend::doPauseResume() {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  pthread_mutex_lock(&audio_data_.lock);
  if (audio_data_.suspendFlag) {
    audio_data_.suspendFlag = false;
    pthread_cond_broadcast(&audio_data_.resumeCond);
    adaptive_delay_.decrease();
  } else {
    audio_data_.suspendFlag = true;
    adaptive_delay_.increase();
  }
  pthread_mutex_unlock(&audio_data_.lock);
  update();
}

waybar::modules::cava::CavaBackend::SignalUpdate&
waybar::modules::cava::CavaBackend::signalUpdate() {
  return m_signal_update_;
}

waybar::modules::cava::CavaBackend::SignalAudioRawUpdate&
waybar::modules::cava::CavaBackend::signalAudioRawUpdate() {
  return m_signal_audio_raw_;
}

waybar::modules::cava::CavaBackend::SignalSilence&
waybar::modules::cava::CavaBackend::signalSilence() {
  return m_signal_silence_;
}

waybar::modules::cava::CavaBackend::SignalConfigChanged&
waybar::modules::cava::CavaBackend::signalConfigChanged() {
  return m_signal_config_changed_;
}

void waybar::modules::cava::CavaBackend::update() { doUpdate(true); }

void waybar::modules::cava::CavaBackend::doUpdate(bool force) {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  if (!plan_ || !input_source_) return;
  if (audio_data_.suspendFlag && !force) return;

  silence_ = isSilent();
  if (!silence_) sleep_counter_ = 0;

  if (silence_ && prm_.sleep_timer != 0) {
    if (sleep_counter_ <=
        static_cast<int>(std::chrono::milliseconds(prm_.sleep_timer * std::chrono::seconds(1)) /
              adaptive_delay_.current())) {
      ++sleep_counter_;
      silence_ = false;
    }
  }

  if (!silence_ || prm_.sleep_timer == 0) {
    while (adaptive_delay_.decrease()) {}
    execute();
    if (re_paint_ == 1 || force || prm_.continuous_rendering) {
      m_signal_update_.emit(output_);
      m_signal_audio_raw_.emit(AudioRaw{audio_raw_});
    }
  } else {
    while (adaptive_delay_.increase()) {}
    if (silence_ != silence_prev_ || force) m_signal_silence_.emit();
  }
  silence_prev_ = silence_;
}

void waybar::modules::cava::CavaBackend::freeBackend() {
  input_source_ = nullptr;

  if (plan_ != nullptr) {
    cava_destroy(plan_);
    plan_ = nullptr;
  }

  if (audio_raw_initialized_) {
    audio_raw_clean(&audio_raw_);
    audio_raw_initialized_ = false;
  }
  pthread_mutex_lock(&audio_data_.lock);
  audio_data_.terminate = 1;
  pthread_mutex_unlock(&audio_data_.lock);
  free_config(&prm_);
  prm_ = {};
  free(audio_data_.source);
  audio_data_.source = nullptr;
  free(audio_data_.cava_in);
  audio_data_.cava_in = nullptr;
}

void waybar::modules::cava::CavaBackend::loadConfig() {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  if (shutdown_.load()) {
    return;
  }

  const Json::Value& cfg = config_;

  struct ::cava::config_params new_prm{};
  CavaConfigGuard new_prm_guard(&new_prm);
  std::vector<char> cfgPath(PATH_MAX, '\0');
  if (cfg["cava_config"].isString()) {
    const std::string& s = cfg["cava_config"].asString();
    auto len = std::min(s.size(), static_cast<size_t>(PATH_MAX - 1));
    std::copy_n(s.begin(), len, cfgPath.begin());
    cfgPath[len] = '\0';
  }
  error_.length = 0;

  if (!load_config(cfgPath.data(), &new_prm, &error_)) {
    throw std::runtime_error(std::string{"cava backend: error loading config: "} + error_.message);
  }

  new_prm.inAtty = 0;
  auto const output{new_prm.output};
  if (new_prm.data_format) free(new_prm.data_format);
  new_prm.data_format = strdup(
      cfg["data_format"].isString() ? cfg["data_format"].asString().c_str() : "ascii");
  if (cfg["raw_target"].isString()) {
    if (new_prm.raw_target) free(new_prm.raw_target);
    new_prm.raw_target = strdup(cfg["raw_target"].asString().c_str());
  }
  {
    auto icon_count = cfg["format-icons"].size();
    new_prm.ascii_range = (icon_count > 0) ? static_cast<int>(icon_count) - 1 : 0;
  }

  if (cfg["bar_spacing"].isInt()) new_prm.bar_spacing = cfg["bar_spacing"].asInt();
  if (cfg["bar_width"].isInt()) new_prm.bar_width = cfg["bar_width"].asInt();
  if (cfg["bar_height"].isInt()) new_prm.bar_height = cfg["bar_height"].asInt();
  new_prm.orientation = ::cava::ORIENT_TOP;
  new_prm.xaxis = ::cava::xaxis_scale::NONE;
  new_prm.mono_opt = ::cava::AVERAGE;
  new_prm.autobars = 0;

  if (cfg["framerate"].isInt()) new_prm.framerate = cfg["framerate"].asInt();
  if (cfg["autosens"].isInt()) new_prm.autosens = cfg["autosens"].asInt();
  if (cfg["sensitivity"].isInt()) new_prm.sens = cfg["sensitivity"].asInt();
  if (cfg["bars"].isInt()) new_prm.fixedbars = cfg["bars"].asInt();
  if (cfg["lower_cutoff_freq"].isNumeric())
    new_prm.lower_cut_off = cfg["lower_cutoff_freq"].asLargestInt();
  if (cfg["higher_cutoff_freq"].isNumeric())
    new_prm.upper_cut_off = cfg["higher_cutoff_freq"].asLargestInt();
  if (cfg["sleep_timer"].isInt()) new_prm.sleep_timer = cfg["sleep_timer"].asInt();
  if (cfg["method"].isString())
    new_prm.input = ::cava::input_method_by_name(cfg["method"].asString().c_str());
  if (cfg["source"].isString()) {
    if (new_prm.audio_source) free(new_prm.audio_source);
    new_prm.audio_source = strdup(cfg["source"].asString().c_str());
  }
  if (cfg["sample_rate"].isNumeric()) new_prm.samplerate = cfg["sample_rate"].asLargestInt();
  if (cfg["sample_bits"].isInt()) new_prm.samplebits = cfg["sample_bits"].asInt();
  if (cfg["stereo"].isBool()) new_prm.stereo = cfg["stereo"].asBool();
  if (cfg["reverse"].isBool()) new_prm.reverse = cfg["reverse"].asBool();
  if (cfg["bar_delimiter"].isInt()) new_prm.bar_delim = cfg["bar_delimiter"].asInt();
  if (cfg["monstercat"].isBool()) new_prm.monstercat = cfg["monstercat"].asBool();
  if (cfg["waves"].isBool()) new_prm.waves = cfg["waves"].asBool();
  if (cfg["noise_reduction"].isDouble())
    new_prm.noise_reduction = cfg["noise_reduction"].asDouble();
  if (cfg["input_delay"].isInt())
    fetch_input_delay_ = std::chrono::seconds(cfg["input_delay"].asInt());
  if (cfg["gradient"].isInt()) new_prm.gradient = cfg["gradient"].asInt();
  if (new_prm.gradient == 0)
    new_prm.gradient_count = 0;
  else if (cfg["gradient_count"].isInt())
    new_prm.gradient_count = cfg["gradient_count"].asInt();
  if (cfg["sdl_width"].isInt()) new_prm.sdl_width = cfg["sdl_width"].asInt();
  if (cfg["sdl_height"].isInt()) new_prm.sdl_height = cfg["sdl_height"].asInt();

  if (new_prm.framerate <= 0) {
    throw std::runtime_error(std::string{"cava backend: framerate must be positive, got: "} +
                             std::to_string(new_prm.framerate));
  }

  adaptive_delay_.reset(std::chrono::milliseconds(static_cast<int>(1e3 / new_prm.framerate)));
  freeBackend();

  try {
    audio_raw_.height = new_prm.ascii_range;
    audio_data_.format = -1;
    audio_data_.rate = 0;
    audio_data_.samples_counter = 0;
    audio_data_.channels = 2;
    audio_data_.IEEE_FLOAT = 0;
    audio_data_.input_buffer_size = BUFFER_SIZE * audio_data_.channels;
    audio_data_.cava_buffer_size = audio_data_.input_buffer_size * 8;
    audio_data_.terminate = 0;
    audio_data_.suspendFlag = false;

    input_source_ = get_input(&audio_data_, &new_prm);
    if (!input_source_) {
      throw std::runtime_error("cava backend: API didn't provide an input audio source");
    }

    new_prm.output = ::cava::output_method::OUTPUT_RAW;

    audio_raw_init(&audio_data_, &audio_raw_, &new_prm, &plan_);
    if (!plan_) {
      throw std::runtime_error("cava backend plan is not provided");
    }
    audio_raw_.previous_frame[0] = -1;
    audio_raw_initialized_ = true;
  } catch (...) {
    freeBackend();
    throw;
  }

  prm_ = new_prm;
  new_prm_guard.release();
  prm_.output = output;

  m_signal_config_changed_.emit();
}

const ::cava::config_params& waybar::modules::cava::CavaBackend::getPrm() const {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  return prm_;
}

std::chrono::milliseconds waybar::modules::cava::CavaBackend::getFrameTimeMilsec() const {
  std::lock_guard<std::recursive_mutex> lock(state_mutex_);
  return adaptive_delay_.current();
}
