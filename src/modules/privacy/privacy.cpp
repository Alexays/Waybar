#include "modules/privacy/privacy.hpp"

#include <json/value.h>
#include <spdlog/spdlog.h>

#include <string>

#include "AModule.hpp"
#include "modules/privacy/privacy_item.hpp"

namespace waybar::modules::privacy {

using waybar::util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_INPUT;
using waybar::util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_OUTPUT;
using waybar::util::PipewireBackend::PRIVACY_NODE_TYPE_LOCATION;
using waybar::util::PipewireBackend::PRIVACY_NODE_TYPE_NONE;
using waybar::util::PipewireBackend::PRIVACY_NODE_TYPE_VIDEO_INPUT;

Privacy::Privacy(const std::string& id, const Json::Value& config, Gtk::Orientation orientation,
                 const std::string& pos)
    : AModule(config, "privacy", id),
      nodes_screenshare(),
      nodes_audio_in(),
      nodes_audio_out(),
      location_in_use(false),
      visibility_conn(),
      geoclue_timeout_conn(),
      box_(orientation, 0) {
  box_.set_name(name_);

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

  // Transition Duration
  if (config_["transition-duration"].isUInt()) {
    transition_duration = config_["transition-duration"].asUInt();
  }

  // Initialize each privacy module
  Json::Value modules = config_["modules"];
  // Add Screenshare, Mic, and Location usage as default modules if none are specified
  if (!modules.isArray() || modules.empty()) {
    modules = Json::Value(Json::arrayValue);
    for (const auto& type : {"screenshare", "audio-in", "location"}) {
      Json::Value obj = Json::Value(Json::objectValue);
      obj["type"] = type;
      modules.append(obj);
    }
  }

  std::map<std::string, std::tuple<std::list<PWPrivacyNodeInfo*>*, PrivacyNodeType>> typeMap = {
      {"screenshare", {&nodes_screenshare, PRIVACY_NODE_TYPE_VIDEO_INPUT}},
      {"audio-in", {&nodes_audio_in, PRIVACY_NODE_TYPE_AUDIO_INPUT}},
      {"audio-out", {&nodes_audio_out, PRIVACY_NODE_TYPE_AUDIO_OUTPUT}},
      {"location", {nullptr, PRIVACY_NODE_TYPE_LOCATION}},
  };

  for (const auto& module : modules) {
    if (!module.isObject() || !module["type"].isString()) continue;
    const std::string type = module["type"].asString();

    auto iter = typeMap.find(type);
    if (iter != typeMap.end()) {
      auto& [nodePtr, nodeType] = iter->second;
      switch (nodeType) {
        case util::PipewireBackend::PRIVACY_NODE_TYPE_VIDEO_INPUT:
        case util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_INPUT:
        case util::PipewireBackend::PRIVACY_NODE_TYPE_AUDIO_OUTPUT:
          box_.add(*Gtk::make_managed<PWPrivacyItem>(module, nodeType, nodePtr, orientation, pos,
                                                     iconSize, transition_duration));
          break;
        case util::PipewireBackend::PRIVACY_NODE_TYPE_LOCATION:
          box_.add(*Gtk::make_managed<GeoCluePrivacyItem>(module, orientation, pos, iconSize,
                                                          transition_duration));
          break;
        case util::PipewireBackend::PRIVACY_NODE_TYPE_NONE:
          continue;
      }
    }
  }

  for (const auto& ignore_item : config_["ignore"]) {
    if (!ignore_item.isObject() || !ignore_item["type"].isString() ||
        !ignore_item["name"].isString())
      continue;
    const std::string type = ignore_item["type"].asString();
    const std::string name = ignore_item["name"].asString();

    auto iter = typeMap.find(type);
    if (iter != typeMap.end()) {
      auto& [_, nodeType] = iter->second;
      ignore.emplace(nodeType, std::move(name));
    }
  }

  if (config_["ignore-monitor"].isBool()) {
    ignore_monitor = config_["ignore-monitor"].asBool();
  }

  pw_backend = util::PipewireBackend::PipewireBackend::getInstance();
  pw_backend->privacy_nodes_changed_signal_event.connect(
      sigc::mem_fun(*this, &Privacy::onPWPrivacyNodesChanged));

  geoclue_backend = util::GeoClueBackend::GeoClueBackend::getInstance();
  geoclue_backend->in_use_changed_signal_event.connect(
      sigc::mem_fun(*this, &Privacy::onGeoCluePrivacyNodesChanged));

  dp.emit();
}

void Privacy::onPWPrivacyNodesChanged() {
  mutex_.lock();
  nodes_audio_out.clear();
  nodes_audio_in.clear();
  nodes_screenshare.clear();

  for (auto& node : pw_backend->privacy_nodes) {
    if (ignore_monitor && node.second->is_monitor) continue;

    auto iter = ignore.find(std::pair(node.second->type, node.second->node_name));
    if (iter != ignore.end()) continue;

    switch (node.second->state) {
      case PW_NODE_STATE_RUNNING:
        switch (node.second->type) {
          case PRIVACY_NODE_TYPE_VIDEO_INPUT:
            nodes_screenshare.push_back(node.second);
            break;
          case PRIVACY_NODE_TYPE_AUDIO_INPUT:
            nodes_audio_in.push_back(node.second);
            break;
          case PRIVACY_NODE_TYPE_AUDIO_OUTPUT:
            nodes_audio_out.push_back(node.second);
            break;
          default:
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

bool Privacy::locationTimeout(bool in_use) {
  location_in_use.store(geoclue_backend->location_in_use);
  dp.emit();

  return false;
}
void Privacy::onGeoCluePrivacyNodesChanged() {
  geoclue_timeout_conn.disconnect();

  const bool in_use = geoclue_backend->location_in_use;
  if (in_use) {
    locationTimeout(in_use);
  } else {
    // The GeoClue service is really fast, so wait before hiding, so that the
    // widget is at least visible for 3 seconds.
    geoclue_timeout_conn = Glib::signal_timeout().connect_seconds(
        sigc::bind(sigc::mem_fun(*this, &Privacy::locationTimeout), in_use), 3);
  }
}

auto Privacy::update() -> void {
  // set in modules or not
  bool setScreenshare = false;
  bool setAudioIn = false;
  bool setAudioOut = false;
  bool setLocation = false;

  // used or not
  bool useScreenshare = false;
  bool useAudioIn = false;
  bool useAudioOut = false;
  bool useLocation = false;

  mutex_.lock();
  for (Gtk::Widget* widget : box_.get_children()) {
    auto* module = dynamic_cast<PrivacyItem*>(widget);
    if (module == nullptr) continue;
    switch (module->privacy_type) {
      case PRIVACY_NODE_TYPE_VIDEO_INPUT:
        setScreenshare = true;
        useScreenshare = !nodes_screenshare.empty();
        module->set_in_use(useScreenshare);
        break;
      case PRIVACY_NODE_TYPE_AUDIO_INPUT:
        setAudioIn = true;
        useAudioIn = !nodes_audio_in.empty();
        module->set_in_use(useAudioIn);
        break;
      case PRIVACY_NODE_TYPE_AUDIO_OUTPUT:
        setAudioOut = true;
        useAudioOut = !nodes_audio_out.empty();
        module->set_in_use(useAudioOut);
        break;
      case PRIVACY_NODE_TYPE_LOCATION:
        setLocation = true;
        useLocation = location_in_use;
        module->set_in_use(useLocation);
        break;
      case PRIVACY_NODE_TYPE_NONE:
        break;
    }
  }
  mutex_.unlock();

  // Hide the whole widget if none are in use
  bool isVisible = (setScreenshare && useScreenshare) || (setAudioIn && useAudioIn) ||
                   (setAudioOut && useAudioOut) || (setLocation && useLocation);

  if (isVisible != event_box_.get_visible()) {
    // Disconnect any previous connection so that it doesn't get activated in
    // the future, hiding the module when it should be visible
    visibility_conn.disconnect();
    if (isVisible) {
      event_box_.set_visible(true);
    } else {
      // Hides the widget when all of the privacy_item revealers animations
      // have finished animating
      visibility_conn = Glib::signal_timeout().connect(
          sigc::track_obj(
              [this, setScreenshare, setAudioOut, setAudioIn, setLocation]() {
                mutex_.lock();
                bool visible = false;
                visible |= setScreenshare && !nodes_screenshare.empty();
                visible |= setAudioIn && !nodes_audio_in.empty();
                visible |= setAudioOut && !nodes_audio_out.empty();
                visible |= setLocation && location_in_use;
                mutex_.unlock();
                event_box_.set_visible(visible);
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
