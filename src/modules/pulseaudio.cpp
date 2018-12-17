#include "modules/pulseaudio.hpp"

waybar::modules::Pulseaudio::Pulseaudio(const Json::Value &config)
    : ALabel(config, "{volume}%"),
      mainloop_(nullptr),
      mainloop_api_(nullptr),
      context_(nullptr),
      sink_idx_(0),
      volume_(0),
      muted_(false),
      scrolling_(false) {
  label_.set_name("pulseaudio");
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
  if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOAUTOSPAWN,
    nullptr) < 0) {
    auto err = fmt::format("pa_context_connect() failed: {}",
      pa_strerror(pa_context_errno(context_)));
    throw std::runtime_error(err);
  }
  pa_context_set_state_callback(context_, contextStateCb, this);
  if (pa_threaded_mainloop_start(mainloop_) < 0) {
    throw std::runtime_error("pa_mainloop_run() failed.");
  }
  pa_threaded_mainloop_unlock(mainloop_);

  // define the pulse scroll events only when no user provided
  // events are configured
  if (!config["on-scroll-up"].isString() &&
      !config["on-scroll-down"].isString()) {
    event_box_.add_events(Gdk::SCROLL_MASK);
    event_box_.signal_scroll_event().connect(
        sigc::mem_fun(*this, &Pulseaudio::handleScroll));
  }
}

waybar::modules::Pulseaudio::~Pulseaudio() {
  mainloop_api_->quit(mainloop_api_, 0);
  pa_threaded_mainloop_stop(mainloop_);
  pa_threaded_mainloop_free(mainloop_);
}

void waybar::modules::Pulseaudio::contextStateCb(pa_context *c, void *data)
{
  auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_TERMINATED:
      pa->mainloop_api_->quit(pa->mainloop_api_, 0);
      break;
    case PA_CONTEXT_READY:
      pa_context_get_server_info(c, serverInfoCb, data);
      pa_context_set_subscribe_callback(c, subscribeCb, data);
      pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, nullptr, nullptr);
      break;
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;
    case PA_CONTEXT_FAILED:
    default:
      pa->mainloop_api_->quit(pa->mainloop_api_, 1);
      break;
  }
}

bool waybar::modules::Pulseaudio::handleScroll(GdkEventScroll *e) {
  // Avoid concurrent scroll event
  bool direction_up = false;
  uint16_t change = config_["scroll-step"].isUInt() ? config_["scroll-step"].asUInt() * 100 : 100;
  pa_cvolume pa_volume = pa_volume_;

  if (scrolling_) {
    return false;
  }
  scrolling_ = true;
  if (e->direction == GDK_SCROLL_UP) {
    direction_up = true;
  }
  if (e->direction == GDK_SCROLL_DOWN) {
    direction_up = false;
  }

  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdouble delta_x, delta_y;
    gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent *>(e), &delta_x,
                                &delta_y);
    if (delta_y < 0) {
      direction_up = true;
    } else if (delta_y > 0) {
      direction_up = false;
    }
  }

  if (direction_up) {
    if (volume_ + 1 < 100) pa_cvolume_inc(&pa_volume, change);
  } else {
    if (volume_ - 1 > 0) pa_cvolume_dec(&pa_volume, change);
  }

  pa_context_set_sink_volume_by_index(context_, sink_idx_, &pa_volume,
                                      volumeModifyCb, this);

  return true;
}

/*
 * Called when an event we subscribed to occurs.
 */
void waybar::modules::Pulseaudio::subscribeCb(pa_context* context,
  pa_subscription_event_type_t type, uint32_t idx, void* data)
{
  unsigned facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;

  switch (facility) {
    case PA_SUBSCRIPTION_EVENT_SINK:
        pa_context_get_sink_info_by_index(context, idx, sinkInfoCb, data);
        break;
    default:
        break;
  }
}

/*
 * Called in response to a volume change request
 */
void waybar::modules::Pulseaudio::volumeModifyCb(pa_context *c, int success,
                                                 void *data) {
  auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
  if (success) {
    pa_context_get_sink_info_by_index(pa->context_, pa->sink_idx_, sinkInfoCb,
                                      data);
  }
}

/*
 * Called when the requested sink information is ready.
 */
void waybar::modules::Pulseaudio::sinkInfoCb(pa_context * /*context*/,
                                             const pa_sink_info *i, int /*eol*/,
                                             void *data) {
  if (i != nullptr) {
    auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
    pa->pa_volume_ = i->volume;
    float volume = static_cast<float>(pa_cvolume_avg(&(pa->pa_volume_))) /
                   float{PA_VOLUME_NORM};
    pa->sink_idx_ = i->index;
    pa->volume_ = std::round(volume * 100.0f);
    pa->muted_ = i->mute != 0;
    pa->desc_ = i->description;
    pa->port_name_ = i->active_port ? i->active_port->name : "Unknown";
    pa->dp.emit();
  }
}

/*
 * Called when the requested information on the server is ready. This is
 * used to find the default PulseAudio sink.
 */
void waybar::modules::Pulseaudio::serverInfoCb(pa_context *context,
  const pa_server_info *i, void *data)
{
  pa_context_get_sink_info_by_name(context, i->default_sink_name,
    sinkInfoCb, data);
}

const std::string waybar::modules::Pulseaudio::getPortIcon() const
{
  std::vector<std::string> ports = {
    "headphones",
    "speaker",
    "hdmi",
    "headset",
    "handsfree",
    "portable",
    "car",
    "hifi",
    "phone",
  };
  for (auto const& port : ports) {
    if (port_name_.find(port) != std::string::npos) {
      return port;
    }
  }
  return "";
}

auto waybar::modules::Pulseaudio::update() -> void
{
  auto format = format_;
  if (muted_) {
    format =
      config_["format-muted"].isString() ? config_["format-muted"].asString() : format;
    label_.get_style_context()->add_class("muted");
  } else if (port_name_.find("a2dp_sink") != std::string::npos) {
    format = config_["format-bluetooth"].isString()
      ? config_["format-bluetooth"].asString() : format;
    label_.get_style_context()->add_class("bluetooth");
  } else {
    label_.get_style_context()->remove_class("muted");
    label_.get_style_context()->add_class("bluetooth");
  }
  label_.set_markup(
      fmt::format(format, fmt::arg("volume", volume_),
                  fmt::arg("icon", getIcon(volume_, getPortIcon()))));
  label_.set_tooltip_text(desc_);
  if (scrolling_) {
    scrolling_ = false;
  }
}
