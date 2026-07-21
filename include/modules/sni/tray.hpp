#pragma once

#include <fmt/format.h>
#include <sigc++/connection.h>

#include <unordered_map>
#include <utility>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/sni/host.hpp"
#include "modules/sni/watcher.hpp"
#include "util/json.hpp"

namespace waybar::modules::SNI {

class Tray : public AModule {
 public:
  Tray(const std::string&, const Bar&, const Json::Value&, std::mutex&, std::list<pid_t>&);
  ~Tray() override = default;
  auto update() -> void override;

 private:
  void onAdd(std::unique_ptr<Item>& item);
  void onRemove(std::unique_ptr<Item>& item);
  // Reorders the already-added tray widgets by their configured order. Does not
  // add or remove any widget.
  void reorderBox();
  void checkIgnoreList(std::unique_ptr<Item>* item);
  std::vector<std::string> parseIgnoreList(const Json::Value& config);
  void queueUpdate();

  static inline std::size_t nb_hosts_ = 0;
  Gtk::Box box_;
  SNI::Watcher::singleton watcher_;
  std::vector<std::string> ignore_list_;
  SNI::Host host_;
  std::vector<Item*> items_;
  // signal_show/signal_hide connections owned per added item, so they can be
  // disconnected on removal instead of leaking and accumulating.
  std::unordered_map<Item*, std::pair<sigc::connection, sigc::connection>> item_connections_;
};

}  // namespace waybar::modules::SNI
