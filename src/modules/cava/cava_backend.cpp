#include "modules/cava/cava_backend.hpp"

#include <spdlog/spdlog.h>

std::shared_ptr<waybar::modules::cava::CavaBackend> waybar::modules::cava::CavaBackend::inst(
    const Json::Value& config) {
  static auto* backend = new CavaBackend(config);
  static std::shared_ptr<CavaBackend> backend_ptr{backend};
  return backend_ptr;
}

waybar::modules::cava::CavaBackend::CavaBackend(const Json::Value& config) {
  // Load waybar module config
  char cfgPath[PATH_MAX];
  cfgPath[0] = '\0';

  if (config["cava_config"].isString()) strcpy(cfgPath, config["cava_config"].asString().data());
  // Load cava config
  error_.length = 0;

  if (!load_config(cfgPath, &prm_, false, &error_)) {
    spdlog::error("cava backend. Error loading config. {0}", error_.message);
    exit(EXIT_FAILURE);
  }

  // Override cava parameters by the user config
  prm_.inAtty = 0;
  prm_.output = ::cava::output_method::OUTPUT_RAW;
  strcpy(prm_.data_format, "ascii");
  strcpy(prm_.raw_target, "/dev/stdout");
  prm_.ascii_range = config["format-icons"].size() - 1;

  prm_.bar_width = 2;
  prm_.bar_spacing = 0;
  prm_.bar_height = 32;
  prm_.bar_width = 1;
  prm_.orientation = ::cava::ORIENT_TOP;
  prm_.xaxis = ::cava::xaxis_scale::NONE;
  prm_.mono_opt = ::cava::AVERAGE;
  prm_.autobars = 0;
  prm_.gravity = 0;
  prm_.integral = 1;

  if (config["framerate"].isInt()) prm_.framerate = config["framerate"].asInt();
  // Calculate delay for Update() thread
  frame_time_milsec_ = std::chrono::milliseconds((int)(1e3 / prm_.framerate));
  if (config["autosens"].isInt()) prm_.autosens = config["autosens"].asInt();
  if (config["sensitivity"].isInt()) prm_.sens = config["sensitivity"].asInt();
  if (config["bars"].isInt()) prm_.fixedbars = config["bars"].asInt();
  if (config["lower_cutoff_freq"].isNumeric())
    prm_.lower_cut_off = config["lower_cutoff_freq"].asLargestInt();
  if (config["higher_cutoff_freq"].isNumeric())
    prm_.upper_cut_off = config["higher_cutoff_freq"].asLargestInt();
  if (config["sleep_timer"].isInt()) prm_.sleep_timer = config["sleep_timer"].asInt();
  if (config["method"].isString())
    prm_.input = ::cava::input_method_by_name(config["method"].asString().c_str());
  if (config["source"].isString()) prm_.audio_source = config["source"].asString().data();
  if (config["sample_rate"].isNumeric()) prm_.samplerate = config["sample_rate"].asLargestInt();
  if (config["sample_bits"].isInt()) prm_.samplebits = config["sample_bits"].asInt();
  if (config["stereo"].isBool()) prm_.stereo = config["stereo"].asBool();
  if (config["reverse"].isBool()) prm_.reverse = config["reverse"].asBool();
  if (config["bar_delimiter"].isInt()) prm_.bar_delim = config["bar_delimiter"].asInt();
  if (config["monstercat"].isBool()) prm_.monstercat = config["monstercat"].asBool();
  if (config["waves"].isBool()) prm_.waves = config["waves"].asBool();
  if (config["noise_reduction"].isDouble())
    prm_.noise_reduction = config["noise_reduction"].asDouble();
  if (config["input_delay"].isInt())
    fetch_input_delay_ = std::chrono::seconds(config["input_delay"].asInt());

  // Make cava parameters configuration
  plan_ = new ::cava::cava_plan{};

  audio_raw_.height = prm_.ascii_range;
  audio_data_.format = -1;
  audio_data_.source = new char[1 + strlen(prm_.audio_source)];
  audio_data_.source[0] = '\0';
  strcpy(audio_data_.source, prm_.audio_source);

  audio_data_.rate = 0;
  audio_data_.samples_counter = 0;
  audio_data_.channels = 2;
  audio_data_.IEEE_FLOAT = 0;

  audio_data_.input_buffer_size = BUFFER_SIZE * audio_data_.channels;
  audio_data_.cava_buffer_size = audio_data_.input_buffer_size * 8;

  audio_data_.cava_in = new double[audio_data_.cava_buffer_size]{0.0};

  audio_data_.terminate = 0;
  audio_data_.suspendFlag = false;
  input_source_ = get_input(&audio_data_, &prm_);

  if (!input_source_) {
    spdlog::error("cava backend API didn't provide input audio source method");
    exit(EXIT_FAILURE);
  }

  // Init cava plan, audio_raw structure
  audio_raw_init(&audio_data_, &audio_raw_, &prm_, plan_);
  if (!plan_) spdlog::error("cava backend plan is not provided");
  audio_raw_.previous_frame[0] = -1;  // For first Update() call need to rePaint text message
  // Read audio source trough cava API. Cava orginizes this process via infinity loop
  read_thread_ = [this] {
    try {
      input_source_(&audio_data_);
    } catch (const std::runtime_error& e) {
      spdlog::warn("Cava backend. Read source error: {0}", e.what());
    }
    read_thread_.sleep_for(fetch_input_delay_);
  };

  thread_ = [this] {
    doUpdate();
    thread_.sleep_for(frame_time_milsec_);
  };
}

waybar::modules::cava::CavaBackend::~CavaBackend() {
  thread_.stop();
  read_thread_.stop();
  delete plan_;
  plan_ = nullptr;
}

static void upThreadDelay(std::chrono::milliseconds& delay, std::chrono::seconds& delta) {
  if (delta == std::chrono::seconds{0}) {
    delta += std::chrono::seconds{1};
    delay += delta;
  }
}

static void downThreadDelay(std::chrono::milliseconds& delay, std::chrono::seconds& delta) {
  if (delta > std::chrono::seconds{0}) {
    delay -= delta;
    delta -= std::chrono::seconds{1};
  }
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
}

waybar::modules::cava::CavaBackend::type_signal_update
waybar::modules::cava::CavaBackend::signal_update() {
  return m_signal_update_;
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
    downThreadDelay(frame_time_milsec_, suspend_silence_delay_);
    execute();
    if (re_paint_ == 1 || force) m_signal_update_.emit(output_);
  } else {
    upThreadDelay(frame_time_milsec_, suspend_silence_delay_);
    if (silence_ != silence_prev_ || force) m_signal_silence_.emit();
  }
  silence_prev_ = silence_;
}
