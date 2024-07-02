#include "modules/cava.hpp"

#include <spdlog/spdlog.h>

waybar::modules::Cava::Cava(const std::string& id, const Json::Value& config)
    : ALabel(config, "cava", id, "{}", 60, false, false, false) {
  // Load waybar module config
  char cfgPath[PATH_MAX];
  cfgPath[0] = '\0';

  if (config_["cava_config"].isString()) strcpy(cfgPath, config_["cava_config"].asString().data());
  // Load cava config
  error_.length = 0;

  if (!load_config(cfgPath, &prm_, false, &error_)) {
    spdlog::error("Error loading config. {0}", error_.message);
    exit(EXIT_FAILURE);
  }

  // Override cava parameters by the user config
  prm_.inAtty = 0;
  prm_.output = cava::output_method::OUTPUT_RAW;
  strcpy(prm_.data_format, "ascii");
  strcpy(prm_.raw_target, "/dev/stdout");
  prm_.ascii_range = config_["format-icons"].size() - 1;

  prm_.bar_width = 2;
  prm_.bar_spacing = 0;
  prm_.bar_height = 32;
  prm_.bar_width = 1;
  prm_.orientation = cava::ORIENT_TOP;
  prm_.xaxis = cava::xaxis_scale::NONE;
  prm_.mono_opt = cava::AVERAGE;
  prm_.autobars = 0;
  prm_.gravity = 0;
  prm_.integral = 1;

  if (config_["framerate"].isInt()) prm_.framerate = config_["framerate"].asInt();
  if (config_["autosens"].isInt()) prm_.autosens = config_["autosens"].asInt();
  if (config_["sensitivity"].isInt()) prm_.sens = config_["sensitivity"].asInt();
  if (config_["bars"].isInt()) prm_.fixedbars = config_["bars"].asInt();
  if (config_["lower_cutoff_freq"].isNumeric())
    prm_.lower_cut_off = config_["lower_cutoff_freq"].asLargestInt();
  if (config_["higher_cutoff_freq"].isNumeric())
    prm_.upper_cut_off = config_["higher_cutoff_freq"].asLargestInt();
  if (config_["sleep_timer"].isInt()) prm_.sleep_timer = config_["sleep_timer"].asInt();
  if (config_["method"].isString())
    prm_.input = cava::input_method_by_name(config_["method"].asString().c_str());
  if (config_["source"].isString()) prm_.audio_source = config_["source"].asString().data();
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
  if (config_["hide_on_silence"].isBool()) hide_on_silence_ = config_["hide_on_silence"].asBool();
  // Make cava parameters configuration
  plan_ = new cava::cava_plan{};

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
    spdlog::error("cava API didn't provide input audio source method");
    exit(EXIT_FAILURE);
  }
  // Calculate delay for Update() thread
  frame_time_milsec_ = std::chrono::milliseconds((int)(1e3 / prm_.framerate));

  // Init cava plan, audio_raw structure
  audio_raw_init(&audio_data_, &audio_raw_, &prm_, plan_);
  if (!plan_) spdlog::error("cava plan is not provided");
  audio_raw_.previous_frame[0] = -1;  // For first Update() call need to rePaint text message
  // Read audio source trough cava API. Cava orginizes this process via infinity loop
  thread_fetch_input_ = [this] {
    thread_fetch_input_.sleep_for(fetch_input_delay_);
    input_source_(&audio_data_);
  };

  thread_ = [this] {
    dp.emit();
    thread_.sleep_for(frame_time_milsec_);
  };
}

waybar::modules::Cava::~Cava() {
  thread_fetch_input_.stop();
  thread_.stop();
  delete plan_;
  plan_ = nullptr;
}

void upThreadDelay(std::chrono::milliseconds& delay, std::chrono::seconds& delta) {
  if (delta == std::chrono::seconds{0}) {
    delta += std::chrono::seconds{1};
    delay += delta;
  }
}

void downThreadDelay(std::chrono::milliseconds& delay, std::chrono::seconds& delta) {
  if (delta > std::chrono::seconds{0}) {
    delay -= delta;
    delta -= std::chrono::seconds{1};
  }
}

auto waybar::modules::Cava::update() -> void {
  if (audio_data_.suspendFlag) return;
  silence_ = true;

  for (int i{0}; i < audio_data_.input_buffer_size; ++i) {
    if (audio_data_.cava_in[i]) {
      silence_ = false;
      sleep_counter_ = 0;
      break;
    }
  }

  if (silence_ && prm_.sleep_timer) {
    if (sleep_counter_ <=
        (int)(std::chrono::milliseconds(prm_.sleep_timer * 1s) / frame_time_milsec_)) {
      ++sleep_counter_;
      silence_ = false;
    }
  }

  if (!silence_) {
    downThreadDelay(frame_time_milsec_, suspend_silence_delay_);
    // Process: execute cava
    pthread_mutex_lock(&audio_data_.lock);
    cava::cava_execute(audio_data_.cava_in, audio_data_.samples_counter, audio_raw_.cava_out,
                       plan_);
    if (audio_data_.samples_counter > 0) audio_data_.samples_counter = 0;
    pthread_mutex_unlock(&audio_data_.lock);

    // Do transformation under raw data
    audio_raw_fetch(&audio_raw_, &prm_, &rePaint_, plan_);

    if (rePaint_ == 1) {
      text_.clear();

      for (int i{0}; i < audio_raw_.number_of_bars; ++i) {
        audio_raw_.previous_frame[i] = audio_raw_.bars[i];
        text_.append(
            getIcon((audio_raw_.bars[i] > prm_.ascii_range) ? prm_.ascii_range : audio_raw_.bars[i],
                    "", prm_.ascii_range + 1));
        if (prm_.bar_delim != 0) text_.push_back(prm_.bar_delim);
      }

      label_.set_markup(text_);
      label_.show();
      ALabel::update();
    }
  } else {
    upThreadDelay(frame_time_milsec_, suspend_silence_delay_);
    if (hide_on_silence_) label_.hide();
  }
}

auto waybar::modules::Cava::doAction(const std::string& name) -> void {
  if ((actionMap_[name])) {
    (this->*actionMap_[name])();
  } else
    spdlog::error("Cava. Unsupported action \"{0}\"", name);
}

// Cava actions
void waybar::modules::Cava::pause_resume() {
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
