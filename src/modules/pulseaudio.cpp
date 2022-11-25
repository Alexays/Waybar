#include "modules/pulseaudio.hpp"

waybar::modules::Pulseaudio::Pulseaudio(const std::string &id, const Json::Value &config)
    : ALabel(config, "pulseaudio", id, "{volume}%"),
      mainloop_(nullptr),
      mainloop_api_(nullptr),
      context_(nullptr),
      sink_idx_(0),
      volume_(0),
      muted_(false),
      source_idx_(0),
      source_volume_(0),
      source_muted_(false) {
  mainloop_ = pa_threaded_mainloop_new();
  if (mainloop_ == nullptr) {
    throw std::runtime_error("pa_mainloop_new() failed.");
  }
  pa_threaded_mainloop_lock(mainloop_);
  mainloop_api_ = pa_threaded_mainloop_get_api(mainloop_);
  context_ = pa_context_new(mainloop_api_, "waybar");
  if (context_ == nullptr) {
    throw std::runtime_error("pa_context_new() failed.");
  }
  if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFAIL, nullptr) < 0) {
    auto err =
        fmt::format("pa_context_connect() failed: {}", pa_strerror(pa_context_errno(context_)));
    throw std::runtime_error(err);
  }
  pa_context_set_state_callback(context_, contextStateCb, this);
  if (pa_threaded_mainloop_start(mainloop_) < 0) {
    throw std::runtime_error("pa_mainloop_run() failed.");
  }
  pa_threaded_mainloop_unlock(mainloop_);
  event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &Pulseaudio::handleScroll));
}

waybar::modules::Pulseaudio::~Pulseaudio() {
  pa_context_disconnect(context_);
  mainloop_api_->quit(mainloop_api_, 0);
  pa_threaded_mainloop_stop(mainloop_);
  pa_threaded_mainloop_free(mainloop_);
}

void waybar::modules::Pulseaudio::contextStateCb(pa_context *c, void *data) {
  auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_TERMINATED:
      pa->mainloop_api_->quit(pa->mainloop_api_, 0);
      break;
    case PA_CONTEXT_READY:
      pa_context_get_server_info(c, serverInfoCb, data);
      pa_context_set_subscribe_callback(c, subscribeCb, data);
      pa_context_subscribe(c,
                           static_cast<enum pa_subscription_mask>(
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SERVER) |
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SINK) |
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SINK_INPUT) |
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SOURCE) |
                               static_cast<int>(PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT)),
                           nullptr, nullptr);
      break;
    case PA_CONTEXT_FAILED:
      pa->mainloop_api_->quit(pa->mainloop_api_, 1);
      break;
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
    default:
      break;
  }
}

bool waybar::modules::Pulseaudio::handleScroll(GdkEventScroll *e) {
  // change the pulse volume only when no user provided
  // events are configured
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(e);
  }
  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }
  if (config_["reverse-scrolling"].asInt() == 1) {
    if (dir == SCROLL_DIR::UP) {
      dir = SCROLL_DIR::DOWN;
    } else if (dir == SCROLL_DIR::DOWN) {
      dir = SCROLL_DIR::UP;
    }
  }
  double volume_tick = static_cast<double>(PA_VOLUME_NORM) / 100;
  pa_volume_t change = volume_tick;
  pa_cvolume pa_volume = pa_volume_;
  int max_volume = 100;
  double step = 1;
  // isDouble returns true for integers as well, just in case
  if (config_["scroll-step"].isDouble()) {
    step = config_["scroll-step"].asDouble();
  }
  if (config_["max-volume"].isInt()) {
    max_volume = std::min(config_["max-volume"].asInt(), static_cast<int>(PA_VOLUME_UI_MAX));
  }

  if (dir == SCROLL_DIR::UP) {
    if (volume_ < max_volume) {
      if (volume_ + step > max_volume) {
        change = round((max_volume - volume_) * volume_tick);
      } else {
        change = round(step * volume_tick);
      }
      pa_cvolume_inc(&pa_volume, change);
    }
  } else if (dir == SCROLL_DIR::DOWN) {
    if (volume_ > 0) {
      if (volume_ - step < 0) {
        change = round(volume_ * volume_tick);
      } else {
        change = round(step * volume_tick);
      }
      pa_cvolume_dec(&pa_volume, change);
    }
  }
  pa_context_set_sink_volume_by_index(context_, sink_idx_, &pa_volume, volumeModifyCb, this);
  return true;
}

/*
 * Called when an event we subscribed to occurs.
 */
void waybar::modules::Pulseaudio::subscribeCb(pa_context *context,
                                              pa_subscription_event_type_t type, uint32_t idx,
                                              void *data) {
  unsigned facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
  unsigned operation = type & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
  if (operation != PA_SUBSCRIPTION_EVENT_CHANGE) {
    return;
  }
  if (facility == PA_SUBSCRIPTION_EVENT_SERVER) {
    pa_context_get_server_info(context, serverInfoCb, data);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SINK) {
    pa_context_get_sink_info_by_index(context, idx, sinkInfoCb, data);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
    pa_context_get_sink_info_list(context, sinkInfoCb, data);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SOURCE) {
    pa_context_get_source_info_by_index(context, idx, sourceInfoCb, data);
  } else if (facility == PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT) {
    pa_context_get_source_info_list(context, sourceInfoCb, data);
  }
}

/*
 * Called in response to a volume change request
 */
void waybar::modules::Pulseaudio::volumeModifyCb(pa_context *c, int success, void *data) {
  auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
  if (success != 0) {
    pa_context_get_sink_info_by_index(pa->context_, pa->sink_idx_, sinkInfoCb, data);
  }
}

/*
 * Called when the requested source information is ready.
 */
void waybar::modules::Pulseaudio::sourceInfoCb(pa_context * /*context*/, const pa_source_info *i,
                                               int /*eol*/, void *data) {
  auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
  if (i != nullptr && pa->default_source_name_ == i->name) {
    auto source_volume = static_cast<float>(pa_cvolume_avg(&(i->volume))) / float{PA_VOLUME_NORM};
    pa->source_volume_ = std::round(source_volume * 100.0F);
    pa->source_idx_ = i->index;
    pa->source_muted_ = i->mute != 0;
    pa->source_desc_ = i->description;
    pa->source_port_name_ = i->active_port != nullptr ? i->active_port->name : "Unknown";
    pa->dp.emit();
  }
}

/*
 * Called when the requested sink information is ready.
 */
void waybar::modules::Pulseaudio::sinkInfoCb(pa_context * /*context*/, const pa_sink_info *i,
                                             int /*eol*/, void *data) {
  if (i == nullptr) return;

  auto pa = static_cast<waybar::modules::Pulseaudio *>(data);

  if (pa->config_["ignored-sinks"].isArray()) {
    for (const auto &ignored_sink : pa->config_["ignored-sinks"]) {
      if (ignored_sink.asString() == i->description) {
        return;
      }
    }
  }

  if (pa->current_sink_name_ == i->name) {
    if (i->state != PA_SINK_RUNNING) {
      pa->current_sink_running_ = false;
    } else {
      pa->current_sink_running_ = true;
    }
  }

  if (!pa->current_sink_running_ && i->state == PA_SINK_RUNNING) {
    pa->current_sink_name_ = i->name;
    pa->current_sink_running_ = true;
  }

  if (pa->current_sink_name_ == i->name) {
    pa->pa_volume_ = i->volume;
    float volume = static_cast<float>(pa_cvolume_avg(&(pa->pa_volume_))) / float{PA_VOLUME_NORM};
    pa->sink_idx_ = i->index;
    pa->volume_ = std::round(volume * 100.0F);
    pa->muted_ = i->mute != 0;
    pa->desc_ = i->description;
    pa->monitor_ = i->monitor_source_name;
    pa->port_name_ = i->active_port != nullptr ? i->active_port->name : "Unknown";
    if (auto ff = pa_proplist_gets(i->proplist, PA_PROP_DEVICE_FORM_FACTOR)) {
      pa->form_factor_ = ff;
    } else {
      pa->form_factor_ = "";
    }
    pa->dp.emit();
  }
}

/*
 * Called when the requested information on the server is ready. This is
 * used to find the default PulseAudio sink.
 */
void waybar::modules::Pulseaudio::serverInfoCb(pa_context *context, const pa_server_info *i,
                                               void *data) {
  auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
  pa->current_sink_name_ = i->default_sink_name;
  pa->default_source_name_ = i->default_source_name;

  pa_context_get_sink_info_list(context, sinkInfoCb, data);
  pa_context_get_source_info_list(context, sourceInfoCb, data);
}

static const std::array<std::string, 9> ports = {
    "headphone", "speaker", "hdmi", "headset", "hands-free", "portable", "car", "hifi", "phone",
};

const std::vector<std::string> waybar::modules::Pulseaudio::getPulseIcon() const {
  std::vector<std::string> res = {current_sink_name_, default_source_name_};
  std::string nameLC = port_name_ + form_factor_;
  std::transform(nameLC.begin(), nameLC.end(), nameLC.begin(), ::tolower);
  for (auto const &port : ports) {
    if (nameLC.find(port) != std::string::npos) {
      res.push_back(port);
      return res;
    }
  }
  return res;
}

auto waybar::modules::Pulseaudio::update() -> void {
  auto format = format_;
  std::string tooltip_format;
  if (!alt_) {
    std::string format_name = "format";
    if (monitor_.find("a2dp_sink") != std::string::npos ||  // PulseAudio
        monitor_.find("a2dp-sink") != std::string::npos ||  // PipeWire
        monitor_.find("bluez") != std::string::npos) {
      format_name = format_name + "-bluetooth";
      label_.get_style_context()->add_class("bluetooth");
    } else {
      label_.get_style_context()->remove_class("bluetooth");
    }
    if (muted_) {
      // Check muted bluetooth format exist, otherwise fallback to default muted format
      if (format_name != "format" && !config_[format_name + "-muted"].isString()) {
        format_name = "format";
      }
      format_name = format_name + "-muted";
      label_.get_style_context()->add_class("muted");
      label_.get_style_context()->add_class("sink-muted");
    } else {
      label_.get_style_context()->remove_class("muted");
      label_.get_style_context()->remove_class("sink-muted");
    }
    format = config_[format_name].isString() ? config_[format_name].asString() : format;
  }
  // TODO: find a better way to split source/sink
  std::string format_source = "{volume}%";
  if (source_muted_) {
    label_.get_style_context()->add_class("source-muted");
    if (config_["format-source-muted"].isString()) {
      format_source = config_["format-source-muted"].asString();
    }
  } else {
    label_.get_style_context()->remove_class("source-muted");
    if (config_["format-source-muted"].isString()) {
      format_source = config_["format-source"].asString();
    }
  }
  format_source = fmt::format(format_source, fmt::arg("volume", source_volume_));
  label_.set_markup(fmt::format(
      format, fmt::arg("desc", desc_), fmt::arg("volume", volume_),
      fmt::arg("format_source", format_source), fmt::arg("source_volume", source_volume_),
      fmt::arg("source_desc", source_desc_), fmt::arg("icon", getIcon(volume_, getPulseIcon()))));
  getState(volume_);

  if (tooltipEnabled()) {
    if (tooltip_format.empty() && config_["tooltip-format"].isString()) {
      tooltip_format = config_["tooltip-format"].asString();
    }
    if (!tooltip_format.empty()) {
      label_.set_tooltip_text(fmt::format(
          tooltip_format, fmt::arg("desc", desc_), fmt::arg("volume", volume_),
          fmt::arg("format_source", format_source), fmt::arg("source_volume", source_volume_),
          fmt::arg("source_desc", source_desc_),
          fmt::arg("icon", getIcon(volume_, getPulseIcon()))));
    } else {
      label_.set_tooltip_text(desc_);
    }
  }

  // Call parent update
  ALabel::update();
}
