#include "modules/privacy/privacy.hpp"

#include <fmt/core.h>
#include <pipewire/pipewire.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "AModule.hpp"
#include "gtkmm/image.h"

namespace waybar::modules::privacy {

using util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_INPUT;
using util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_OUTPUT;
using util::PipewireBackend::PRIVACY_NODE_TYPE_NONE;
using util::PipewireBackend::PRIVACY_NODE_TYPE_VIDEO_INPUT;

Privacy::Privacy(const std::string& id, const Json::Value& config, const std::string& pos)
    : AModule(config, "privacy", id),
      nodes_screenshare(),
      nodes_audio_in(),
      nodes_audio_out(),
      privacy_item_screenshare(config["screenshare"], PRIVACY_NODE_TYPE_VIDEO_INPUT, pos),
      privacy_item_audio_input(config["audio-in"], PRIVACY_NODE_TYPE_AUDIO_INPUT, pos),
      privacy_item_audio_output(config["audio-out"], PRIVACY_NODE_TYPE_AUDIO_OUTPUT, pos),
      visibility_conn(),
      box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  box_.set_name(name_);
  box_.add(privacy_item_screenshare);
  box_.add(privacy_item_audio_output);
  box_.add(privacy_item_audio_input);

  event_box_.add(box_);

  // Icon Spacing
  if (config_["icon-spacing"].isUInt()) {
    iconSpacing = config_["icon-spacing"].asUInt();
  }
  box_.set_spacing(iconSpacing);

  // Icon Size
  if (config_["icon-size"].isUInt()) {
    iconSize = config_["icon-size"].asUInt();
  }
  privacy_item_screenshare.set_icon_size(iconSize);
  privacy_item_audio_output.set_icon_size(iconSize);
  privacy_item_audio_input.set_icon_size(iconSize);

  // Transition Duration
  if (config_["transition-duration"].isUInt()) {
    transition_duration = config_["transition-duration"].asUInt();
  }
  privacy_item_screenshare.set_transition_duration(transition_duration);
  privacy_item_audio_output.set_transition_duration(transition_duration);
  privacy_item_audio_input.set_transition_duration(transition_duration);

  if (!privacy_item_screenshare.is_enabled() && !privacy_item_audio_input.is_enabled() &&
      !privacy_item_audio_output.is_enabled()) {
    throw std::runtime_error("No privacy modules enabled");
  }
  backend = util::PipewireBackend::PipewireBackend::getInstance();
  backend->privacy_nodes_changed_signal_event.connect(
      sigc::mem_fun(*this, &Privacy::onPrivacyNodesChanged));

  dp.emit();
}

void Privacy::onPrivacyNodesChanged() {
  mutex_.lock();
  nodes_audio_out.clear();
  nodes_audio_in.clear();
  nodes_screenshare.clear();

  bool screenshare = false;
  bool audio_in = false;
  bool audio_out = false;
  for (auto& node : backend->privacy_nodes) {
    if (screenshare && audio_in && audio_out) break;
    switch (node.second->state) {
      case PW_NODE_STATE_RUNNING:
        switch (node.second->type) {
          case PRIVACY_NODE_TYPE_VIDEO_INPUT:
            screenshare = true;
            nodes_screenshare.push_back(node.second);
            break;
          case PRIVACY_NODE_TYPE_AUDIO_INPUT:
            audio_in = true;
            nodes_audio_in.push_back(node.second);
            break;
          case PRIVACY_NODE_TYPE_AUDIO_OUTPUT:
            audio_out = true;
            nodes_audio_out.push_back(node.second);
            break;
          case PRIVACY_NODE_TYPE_NONE:
            continue;
        }
        break;
      default:
        break;
    }
  }

  mutex_.unlock();
  dp.emit();
}

auto Privacy::update() -> void {
  bool screenshare = !nodes_screenshare.empty();
  bool audio_in = !nodes_audio_in.empty();
  bool audio_out = !nodes_audio_out.empty();

  privacy_item_screenshare.set_in_use(screenshare);
  privacy_item_audio_input.set_in_use(audio_in);
  privacy_item_audio_output.set_in_use(audio_out);

  // Hide the whole widget if none are in use
  bool is_visible = screenshare || audio_in || audio_out;
  if (is_visible != event_box_.get_visible()) {
    // Disconnect any previous connection so that it doesn't get activated in
    // the future, hiding the module when it should be visible
    visibility_conn.disconnect();
    if (is_visible) {
      event_box_.set_visible(true);
    } else {
      visibility_conn = Glib::signal_timeout().connect(
          sigc::track_obj(
              [this] {
                bool screenshare = !nodes_screenshare.empty();
                bool audio_in = !nodes_audio_in.empty();
                bool audio_out = !nodes_audio_out.empty();
                event_box_.set_visible(screenshare || audio_in || audio_out);
                return false;
              },
              *this),
          transition_duration);
    }
  }

  // Call parent update
  AModule::update();
}

}  // namespace waybar::modules::privacy
