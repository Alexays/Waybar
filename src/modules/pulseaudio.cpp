#include "modules/pulseaudio.hpp"

waybar::modules::Pulseaudio::Pulseaudio(const Json::Value& config)
  : ALabel(config, "{volume}%"), mainloop_(nullptr), mainloop_api_(nullptr),
    context_(nullptr), sink_idx_(0), volume_(0), muted_(false)
{
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
}

waybar::modules::Pulseaudio::~Pulseaudio()
{
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
 * Called when the requested sink information is ready.
 */
void waybar::modules::Pulseaudio::sinkInfoCb(pa_context* /*context*/,
  const pa_sink_info* i, int /*eol*/, void* data)
{
  if (i != nullptr) {
    auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
    float volume = static_cast<float>(pa_cvolume_avg(&(i->volume)))
      / float{PA_VOLUME_NORM};
    pa->sink_idx_ = i->index;
    pa->volume_ = volume * 100.0f;
    pa->muted_ = i->mute != 0;
    pa->desc_ = i->description;
    pa->port_name_ = i->active_port->name;
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
      config_["format-muted"] ? config_["format-muted"].asString() : format;
    label_.get_style_context()->add_class("muted");
  } else if (port_name_.find("a2dp_sink") != std::string::npos) {
    format = config_["format-bluetooth"]
      ? config_["format-bluetooth"].asString() : format;
    label_.get_style_context()->add_class("bluetooth");
  } else {
    label_.get_style_context()->remove_class("muted");
    label_.get_style_context()->add_class("bluetooth");
  }
  label_.set_label(fmt::format(format,
    fmt::arg("volume", volume_),
    fmt::arg("icon", getIcon(volume_, getPortIcon()))));
  label_.set_tooltip_text(desc_);
}
