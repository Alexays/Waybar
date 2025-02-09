#pragma once

#include <gtkmm/box.h>

#include "AModule.hpp"
#include "util/pipewire/pipewire_backend.hpp"

using waybar::util::PipewireBackend::PrivacyNodeInfo;

namespace waybar::modules::privacy {

class Privacy final : public AModule {
 public:
  Privacy(const std::string &, const Json::Value &, const std::string &pos);
  auto update() -> void override;
  Gtk::Widget &root() override;

  void onPrivacyNodesChanged();

 private:
  std::list<PrivacyNodeInfo *> nodes_screenshare;  // Screen is being shared
  std::list<PrivacyNodeInfo *> nodes_audio_in;     // Application is using the microphone
  std::list<PrivacyNodeInfo *> nodes_audio_out;    // Application is outputting audio

  std::mutex mutex_;
  sigc::connection visibility_conn;

  // Config
  Gtk::Box box_;
  uint iconSpacing = 4;
  uint iconSize = 20;
  uint transition_duration = 250;

  std::shared_ptr<util::PipewireBackend::PipewireBackend> backend = nullptr;
};

}  // namespace waybar::modules::privacy
