#pragma once

#include <fmt/format.h>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/sni/host.hpp"
#include "modules/sni/watcher.hpp"
#include "util/json.hpp"

namespace waybar::modules::SNI {

class Tray : public AModule {
 public:
  Tray(const std::string&, const Bar&, const Json::Value&);
  virtual ~Tray() = default;
  auto update() -> void override;

 private:
  void onAdd(std::unique_ptr<Item>& item);
  void onRemove(std::unique_ptr<Item>& item);
  void checkIgnoreList(std::unique_ptr<Item>* item);
  std::vector<std::string> parseIgnoreList(const Json::Value& config);

  static inline std::size_t nb_hosts_ = 0;
  bool show_passive_ = false;
  Gtk::Box box_;
  SNI::Watcher::singleton watcher_;
  std::vector<std::string> ignore_list_;
  SNI::Host host_;
};

}  // namespace waybar::modules::SNI
