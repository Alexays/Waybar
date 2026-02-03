#pragma once

#include <atomic>
#include <string>

#include "gtkmm/box.h"
#include "util/geoclue_backend.hpp"
#include "util/pipewire/pipewire_backend.hpp"
#include "util/pipewire/privacy_node_info.hpp"

using waybar::util::PipewireBackend::PrivacyNodeType;
using waybar::util::PipewireBackend::PWPrivacyNodeInfo;

namespace waybar::modules::privacy {

class Privacy : public AModule {
 public:
  Privacy(const std::string&, const Json::Value&, Gtk::Orientation, const std::string& pos);
  auto update() -> void override;

 private:
  std::list<PWPrivacyNodeInfo*> nodes_screenshare;  // Screen is being shared
  std::list<PWPrivacyNodeInfo*> nodes_audio_in;     // Application is using the microphone
  std::list<PWPrivacyNodeInfo*> nodes_audio_out;    // Application is outputting audio
  std::atomic<bool> location_in_use;                // GeoClue is being used

  std::mutex mutex_;
  sigc::connection visibility_conn;
  sigc::connection geoclue_timeout_conn;

  // Config
  Gtk::Box box_;
  uint iconSpacing = 4;
  uint iconSize = 20;
  uint transition_duration = 250;
  std::set<std::pair<PrivacyNodeType, std::string>> ignore;
  bool ignore_monitor = true;

  std::shared_ptr<util::PipewireBackend::PipewireBackend> pw_backend = nullptr;
  std::shared_ptr<util::GeoClueBackend::GeoClueBackend> geoclue_backend = nullptr;

  void onPWPrivacyNodesChanged();
  bool locationTimeout(bool in_use);
  void onGeoCluePrivacyNodesChanged();
};

}  // namespace waybar::modules::privacy
