#include "modules/cava/cava_backend.hpp"

#include <spdlog/spdlog.h>

std::shared_ptr<waybar::modules::cava::CavaBackend> waybar::modules::cava::CavaBackend::inst(
    const Json::Value& config) {
  static auto* backend = new CavaBackend(config);
  static std::shared_ptr<CavaBackend> backend_ptr{backend};
  return backend_ptr;
}

waybar::modules::cava::CavaBackend::CavaBackend(const Json::Value& config) : config_(config) {
  // Load waybar module config
  loadConfig();
  // Read audio source trough cava API. Cava orginizes this process via infinity loop
  read_thread_ = [this] {
    try {
      input_source_(&audio_data_);
    } catch (const std::runtime_error& e) {
      spdlog::warn("Cava backend. Read source error: {0}", e.what());
    }
    read_thread_.sleep_for(fetch_input_delay_);
    loadConfig();
  };
  // Write outcoming data. Emit signals
  out_thread_ = [this] {
    doUpdate(false);
    out_thread_.sleep_for(frame_time_milsec_);
  };
}

waybar::modules::cava::CavaBackend::~CavaBackend() {
  out_thread_.stop();
  read_thread_.stop();

  freeBackend();
}

static bool upThreadDelay(std::chrono::milliseconds& delay, std::chrono::seconds& delta) {
  if (delta == std::chrono::seconds{0}) {
    delta += std::chrono::seconds{1};
    delay += delta;
    return true;
  }
  return false;
}

static bool downThreadDelay(std::chrono::milliseconds& delay, std::chrono::seconds& delta) {
  if (delta > std::chrono::seconds{0}) {
    delay -= delta;
    delta -= std::chrono::seconds{1};
    return true;
  }
  return false;
}

bool waybar::modules::cava::CavaBackend::isSilence() {
  for (int i{0}; i < audio_data_.input_buffer_size; ++i) {
    if (audio_data_.cava_in[i]) {
      return false;
    }
  }

  return true;
}

int waybar::modules::cava::CavaBackend::getAsciiRange() { return prm_.ascii_range; }

// Process: execute cava
void waybar::modules::cava::CavaBackend::invoke() {
  pthread_mutex_lock(&audio_data_.lock);
  ::cava::cava_execute(audio_data_.cava_in, audio_data_.samples_counter, audio_raw_.cava_out,
                       plan_);
  if (audio_data_.samples_counter > 0) audio_data_.samples_counter = 0;
  pthread_mutex_unlock(&audio_data_.lock);
}

// Do transformation under raw data
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
  pthread_mutex_lock(&audio_data_.lock);
  if (audio_data_.suspendFlag) {
    audio_data_.suspendFlag = false;
    pthread_cond_broadcast(&audio_data_.resumeCond);
    downThreadDelay(frame_time_milsec_, suspend_silence_delay_);
  } else {
    audio_data_.suspendFlag = true;
    upThreadDelay(frame_time_milsec_, suspend_silence_delay_);
  }
  pthread_mutex_unlock(&audio_data_.lock);
  Update();
}

waybar::modules::cava::CavaBackend::type_signal_update
waybar::modules::cava::CavaBackend::signal_update() {
  return m_signal_update_;
}

waybar::modules::cava::CavaBackend::type_signal_audio_raw_update
waybar::modules::cava::CavaBackend::signal_audio_raw_update() {
  return m_signal_audio_raw_;
}

waybar::modules::cava::CavaBackend::type_signal_silence
waybar::modules::cava::CavaBackend::signal_silence() {
  return m_signal_silence_;
}

void waybar::modules::cava::CavaBackend::Update() { doUpdate(true); }

void waybar::modules::cava::CavaBackend::doUpdate(bool force) {
  if (audio_data_.suspendFlag && !force) return;

  silence_ = isSilence();
  if (!silence_) sleep_counter_ = 0;

  if (silence_ && prm_.sleep_timer != 0) {
    if (sleep_counter_ <=
        (int)(std::chrono::milliseconds(prm_.sleep_timer * 1s) / frame_time_milsec_)) {
      ++sleep_counter_;
      silence_ = false;
    }
  }

  if (!silence_ || prm_.sleep_timer == 0) {
    if (downThreadDelay(frame_time_milsec_, suspend_silence_delay_)) Update();
    execute();
    if (re_paint_ == 1 || force || prm_.continuous_rendering) {
      m_signal_update_.emit(output_);
      m_signal_audio_raw_.emit(audio_raw_);
    }
  } else {
    if (upThreadDelay(frame_time_milsec_, suspend_silence_delay_)) Update();
    if (silence_ != silence_prev_ || force) m_signal_silence_.emit();
  }
  silence_prev_ = silence_;
}

void waybar::modules::cava::CavaBackend::freeBackend() {
  if (plan_ != NULL) {
    cava_destroy(plan_);
    plan_ = NULL;
  }

  audio_raw_clean(&audio_raw_);
  pthread_mutex_lock(&audio_data_.lock);
  audio_data_.terminate = 1;
  pthread_mutex_unlock(&audio_data_.lock);
  free_config(&prm_);
  free(audio_data_.source);
  free(audio_data_.cava_in);
}

void waybar::modules::cava::CavaBackend::loadConfig() {
  freeBackend();
  // Load waybar module config
  char cfgPath[PATH_MAX];
  cfgPath[0] = '\0';

  if (config_["cava_config"].isString()) strcpy(cfgPath, config_["cava_config"].asString().data());
  // Load cava config
  error_.length = 0;

  if (!load_config(cfgPath, &prm_, &error_)) {
    spdlog::error("cava backend. Error loading config. {0}", error_.message);
    exit(EXIT_FAILURE);
  }

  // Override cava parameters by the user config
  prm_.inAtty = 0;
  auto const output{prm_.output};
  // prm_.output = ::cava::output_method::OUTPUT_RAW;
  if (prm_.data_format) free(prm_.data_format);
  // Default to ascii for format-icons output; allow user override
  prm_.data_format = strdup(
      config_["data_format"].isString() ? config_["data_format"].asString().c_str() : "ascii");
  if (config_["raw_target"].isString()) {
    if (prm_.raw_target) free(prm_.raw_target);
    prm_.raw_target = strdup(config_["raw_target"].asString().c_str());
  }
  prm_.ascii_range = config_["format-icons"].size() - 1;

  if (config_["bar_spacing"].isInt()) prm_.bar_spacing = config_["bar_spacing"].asInt();
  if (config_["bar_width"].isInt()) prm_.bar_width = config_["bar_width"].asInt();
  if (config_["bar_height"].isInt()) prm_.bar_height = config_["bar_height"].asInt();
  prm_.orientation = ::cava::ORIENT_TOP;
  prm_.xaxis = ::cava::xaxis_scale::NONE;
  prm_.mono_opt = ::cava::AVERAGE;
  prm_.autobars = 0;
  if (config_["gravity"].isInt()) prm_.gravity = config_["gravity"].asInt();
  if (config_["integral"].isInt()) prm_.integral = config_["integral"].asInt();

  if (config_["framerate"].isInt()) prm_.framerate = config_["framerate"].asInt();
  // Calculate delay for Update() thread
  frame_time_milsec_ = std::chrono::milliseconds((int)(1e3 / prm_.framerate));
  if (config_["autosens"].isInt()) prm_.autosens = config_["autosens"].asInt();
  if (config_["sensitivity"].isInt()) prm_.sens = config_["sensitivity"].asInt();
  if (config_["bars"].isInt()) prm_.fixedbars = config_["bars"].asInt();
  if (config_["lower_cutoff_freq"].isNumeric())
    prm_.lower_cut_off = config_["lower_cutoff_freq"].asLargestInt();
  if (config_["higher_cutoff_freq"].isNumeric())
    prm_.upper_cut_off = config_["higher_cutoff_freq"].asLargestInt();
  if (config_["sleep_timer"].isInt()) prm_.sleep_timer = config_["sleep_timer"].asInt();
  if (config_["method"].isString())
    prm_.input = ::cava::input_method_by_name(config_["method"].asString().c_str());
  if (config_["source"].isString()) {
    if (prm_.audio_source) free(prm_.audio_source);
    prm_.audio_source = config_["source"].asString().data();
  }
  if (config_["sample_rate"].isNumeric()) prm_.samplerate = config_["sample_rate"].asLargestInt();
  if (config_["sample_bits"].isInt()) prm_.samplebits = config_["sample_bits"].asInt();
  if (config_["stereo"].isBool()) prm_.stereo = config_["stereo"].asBool();
  if (config_["reverse"].isBool()) prm_.reverse = config_["reverse"].asBool();
  if (config_["bar_delimiter"].isInt()) prm_.bar_delim = config_["bar_delimiter"].asInt();
  if (config_["monstercat"].isBool()) prm_.monstercat = config_["monstercat"].asBool();
  if (config_["waves"].isBool()) prm_.waves = config_["waves"].asBool();
  if (config_["noise_reduction"].isDouble())
    prm_.noise_reduction = config_["noise_reduction"].asDouble();
  if (config_["input_delay"].isInt())
    fetch_input_delay_ = std::chrono::seconds(config_["input_delay"].asInt());
  if (config_["gradient"].isInt()) prm_.gradient = config_["gradient"].asInt();
  if (prm_.gradient == 0)
    prm_.gradient_count = 0;
  else if (config_["gradient_count"].isInt())
    prm_.gradient_count = config_["gradient_count"].asInt();
  if (config_["sdl_width"].isInt()) prm_.sdl_width = config_["sdl_width"].asInt();
  if (config_["sdl_height"].isInt()) prm_.sdl_height = config_["sdl_height"].asInt();

  audio_raw_.height = prm_.ascii_range;
  audio_data_.format = -1;
  audio_data_.rate = 0;
  audio_data_.samples_counter = 0;
  audio_data_.channels = 2;
  audio_data_.IEEE_FLOAT = 0;
  audio_data_.input_buffer_size = BUFFER_SIZE * audio_data_.channels;
  audio_data_.cava_buffer_size = audio_data_.input_buffer_size * 8;
  audio_data_.terminate = 0;
  audio_data_.suspendFlag = false;
  input_source_ = get_input(&audio_data_, &prm_);

  if (!input_source_) {
    spdlog::error("cava backend API didn't provide input audio source method");
    exit(EXIT_FAILURE);
  }

  prm_.output = ::cava::output_method::OUTPUT_RAW;

  // Make cava parameters configuration
  // Init cava plan, audio_raw structure
  audio_raw_init(&audio_data_, &audio_raw_, &prm_, &plan_);
  if (!plan_) spdlog::error("cava backend plan is not provided");
  audio_raw_.previous_frame[0] = -1;  // For first Update() call need to rePaint text message

  prm_.output = output;
}

const struct ::cava::config_params* waybar::modules::cava::CavaBackend::getPrm() { return &prm_; }
std::chrono::milliseconds waybar::modules::cava::CavaBackend::getFrameTimeMilsec() {
  return frame_time_milsec_;
};
