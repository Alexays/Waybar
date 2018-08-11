#include "modules/pulseaudio.hpp"

waybar::modules::Pulseaudio::Pulseaudio(Json::Value config)
  : _config(config), _mainloop(nullptr), _mainloop_api(nullptr),
    _context(nullptr), _sinkIdx(0), _volume(0), _muted(false)
{
  _label.get_style_context()->add_class("pulseaudio");
  _mainloop = pa_threaded_mainloop_new();
  if (!_mainloop)
    throw std::runtime_error("pa_mainloop_new() failed.");
  pa_threaded_mainloop_lock(_mainloop);
  _mainloop_api = pa_threaded_mainloop_get_api(_mainloop);
  _context = pa_context_new(_mainloop_api, "waybar");
  if (!_context)
    throw std::runtime_error("pa_context_new() failed.");
  if (pa_context_connect(_context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0)
    throw std::runtime_error(fmt::format("pa_context_connect() failed: {}",
      pa_strerror(pa_context_errno(_context))));
  pa_context_set_state_callback(_context, _contextStateCb, this);
  if (pa_threaded_mainloop_start(_mainloop) < 0)
    throw std::runtime_error("pa_mainloop_run() failed.");
  pa_threaded_mainloop_unlock(_mainloop);
};

void waybar::modules::Pulseaudio::_contextStateCb(pa_context *c, void *data)
{
  auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
  switch (pa_context_get_state(c)) {
    case PA_CONTEXT_TERMINATED:
      pa->_mainloop_api->quit(pa->_mainloop_api, 0);
      break;
    case PA_CONTEXT_READY:
      pa_context_get_server_info(c, _serverInfoCb, data);
      pa_context_set_subscribe_callback(c, _subscribeCb, data);
      pa_context_subscribe(c, PA_SUBSCRIPTION_MASK_SINK, nullptr,
        nullptr);
      break;
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
      break;
    case PA_CONTEXT_FAILED:
    default:
      pa->_mainloop_api->quit(pa->_mainloop_api, 1);
      break;
  }
}

/*
 * Called when an event we subscribed to occurs.
 */
void waybar::modules::Pulseaudio::_subscribeCb(pa_context *context,
  pa_subscription_event_type_t type, uint32_t idx, void *data)
{
  unsigned facility = type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
  pa_operation *op = nullptr;

  switch (facility) {
      case PA_SUBSCRIPTION_EVENT_SINK:
          pa_context_get_sink_info_by_index(context, idx, _sinkInfoCb, data);
          break;
      default:
          assert(0);
          break;
  }
  if (op)
    pa_operation_unref(op);
}

/*
 * Called when the requested sink information is ready.
 */
void waybar::modules::Pulseaudio::_sinkInfoCb(pa_context *context,
  const pa_sink_info *i, int eol, void *data)
{
  if (i) {
    auto pa = static_cast<waybar::modules::Pulseaudio *>(data);
    float volume = (float)pa_cvolume_avg(&(i->volume)) / (float)PA_VOLUME_NORM;
    pa->_sinkIdx = i->index;
    pa->_volume = volume * 100.0f;
    pa->_muted = i->mute;
    pa->_desc = i->description;
    Glib::signal_idle().connect_once(sigc::mem_fun(*pa, &Pulseaudio::update));
  }
}

/*
 * Called when the requested information on the server is ready. This is
 * used to find the default PulseAudio sink.
 */
void waybar::modules::Pulseaudio::_serverInfoCb(pa_context *context,
  const pa_server_info *i, void *data)
{
    pa_context_get_sink_info_by_name(context, i->default_sink_name, _sinkInfoCb,
      data);
}

auto waybar::modules::Pulseaudio::update() -> void
{
	  auto format = _config["format"] ? _config["format"].asString() : "{}%";
    if (_muted) {
      format =
        _config["format-muted"] ? _config["format-muted"].asString() : format;
      _label.get_style_context()->add_class("muted");
    } else
      _label.get_style_context()->remove_class("muted");
    _label.set_label(fmt::format(format, _volume));
    _label.set_tooltip_text(_desc);
}

waybar::modules::Pulseaudio::operator Gtk::Widget &() {
  return _label;
}
