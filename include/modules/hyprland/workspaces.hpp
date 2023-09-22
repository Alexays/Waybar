#pragma once

#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/enum.hpp"

using WindowAddress = std::string;
using mywindowtype = std::string;
namespace waybar::modules::hyprland {

class Workspaces;

class Workspace {
 public:
  explicit Workspace(const Json::Value& workspace_data, Workspaces& workspace_manager,
                     const Json::Value& clients_json = Json::Value::nullRef);
  std::string& select_icon(std::map<std::string, std::string>& icons_map);
  Gtk::Button& button() { return button_; };

  int id() const { return id_; };
  std::string name() const { return name_; };
  std::string output() const { return output_; };
  bool active() const { return active_; };
  bool is_special() const { return is_special_; };
  bool is_persistent() const { return is_persistent_; };
  bool is_visible() const { return is_visible_; };
  bool is_empty() const { return windows_ == 0; };
  bool is_urgent() const { return is_urgent_; };

  auto handle_clicked(GdkEventButton* bt) -> bool;
  void set_active(bool value = true) { active_ = value; };
  void set_persistent(bool value = true) { is_persistent_ = value; };
  void set_urgent(bool value = true) { is_urgent_ = value; };
  void set_visible(bool value = true) { is_visible_ = value; };
  void set_windows(uint value) { windows_ = value; };
  void set_name(std::string value) { name_ = value; };
  bool contains_window(WindowAddress addr) { return window_map_.contains(addr); }
  void insert_window(WindowAddress addr, mywindowtype window_repr) {
    window_map_.emplace(addr, window_repr);
  };
  void remove_window(WindowAddress addr) { window_map_.erase(addr); }
  void initialize_window_map(const Json::Value& clients_data);

  bool on_window_opened(WindowAddress& addr, std::string& workspace_name,
                        const Json::Value& clients_data);
  bool on_window_opened(WindowAddress& addr, std::string& workspace_name, std::string& window_class,
                        std::string& window_title);

  bool on_window_closed(WindowAddress& addr);
  bool on_window_moved(WindowAddress& addr, std::string& workspace_name,
                       const Json::Value& clients_data);

  void update(const std::string& format, const std::string& icon);

 private:
  Workspaces& workspace_manager_;

  int id_;
  std::string name_;
  std::string output_;
  uint windows_;
  bool active_ = false;
  bool is_special_ = false;
  bool is_persistent_ = false;
  bool is_urgent_ = false;
  bool is_visible_ = false;

  std::map<WindowAddress, mywindowtype> window_map_;

  Gtk::Button button_;
  Gtk::Box content_;
  Gtk::Label label_;
};

class Workspaces : public AModule, public EventHandler {
 public:
  Workspaces(const std::string&, const waybar::Bar&, const Json::Value&);
  ~Workspaces() override;
  void update() override;
  void init();

  auto all_outputs() const -> bool { return all_outputs_; }
  auto show_special() const -> bool { return show_special_; }
  auto active_only() const -> bool { return active_only_; }

  auto get_bar_output() const -> std::string { return bar_.output->name; }

 private:
  void onEvent(const std::string&) override;
  void update_window_count();
  void initialize_window_maps();
  void sort_workspaces();
  void create_workspace(Json::Value& workspace_data,
                        const Json::Value& clients_data = Json::Value::nullRef);
  void remove_workspace(std::string name);
  void set_urgent_workspace(std::string windowaddress);
  void parse_config(const Json::Value& config);
  void register_ipc();

  void on_window_opened(std::string payload);
  void on_window_closed(std::string payload);
  void on_window_moved(std::string payload);

  bool all_outputs_ = false;
  bool show_special_ = false;
  bool active_only_ = false;

  enum class SORT_METHOD { ID, NAME, NUMBER, DEFAULT };
  util::EnumParser<SORT_METHOD> enum_parser_;
  SORT_METHOD sort_by_ = SORT_METHOD::DEFAULT;
  std::map<std::string, SORT_METHOD> sort_map_ = {{"ID", SORT_METHOD::ID},
                                                  {"NAME", SORT_METHOD::NAME},
                                                  {"NUMBER", SORT_METHOD::NUMBER},
                                                  {"DEFAULT", SORT_METHOD::DEFAULT}};

  void fill_persistent_workspaces();
  void create_persistent_workspaces();
  std::vector<std::string> persistent_workspaces_to_create_;
  bool persistent_created_ = false;

  std::string format_;
  std::map<std::string, std::string> icons_map_;
  std::map<std::string, std::string> window_rewrite_rules_;
  std::string format_window_separator_;
  bool with_icon_;
  uint64_t monitor_id_;
  std::string active_workspace_name_;
  std::vector<std::unique_ptr<Workspace>> workspaces_;
  std::vector<Json::Value> workspaces_to_create_;
  std::vector<std::string> workspaces_to_remove_;
  std::mutex mutex_;
  const Bar& bar_;
  Gtk::Box box_;
};

}  // namespace waybar::modules::hyprland
