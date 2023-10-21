#pragma once

#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <json/value.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <variant>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "modules/hyprland/backend.hpp"
#include "util/enum.hpp"
#include "util/regex_collection.hpp"

using WindowAddress = std::string;

namespace waybar::modules::hyprland {

class Workspaces;

class WindowCreationPayload {
 public:
  WindowCreationPayload(std::string workspace_name, WindowAddress window_address,
                        std::string window_repr);
  WindowCreationPayload(std::string workspace_name, WindowAddress window_address,
                        std::string window_class, std::string window_title);
  WindowCreationPayload(Json::Value const& client_data);

  int increment_time_spent_uncreated();
  bool is_empty(Workspaces& workspace_manager);
  bool repr_is_ready() const { return std::holds_alternative<Repr>(window_); }
  std::string repr(Workspaces& workspace_manager);

  std::string workspace_name() const { return workspace_name_; }
  WindowAddress addr() const { return window_address_; }

  void move_to_worksace(std::string& new_workspace_name);

 private:
  void clear_addr();
  void clear_workspace_name();

  using Repr = std::string;
  using ClassAndTitle = std::pair<std::string, std::string>;
  std::variant<Repr, ClassAndTitle> window_;

  WindowAddress window_address_;
  std::string workspace_name_;

  int time_spent_uncreated_ = 0;
};

class Workspace {
 public:
  explicit Workspace(const Json::Value& workspace_data, Workspaces& workspace_manager,
                     const Json::Value& clients_data = Json::Value::nullRef);
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

  bool handle_clicked(GdkEventButton* bt) const;
  void set_active(bool value = true) { active_ = value; };
  void set_persistent(bool value = true) { is_persistent_ = value; };
  void set_urgent(bool value = true) { is_urgent_ = value; };
  void set_visible(bool value = true) { is_visible_ = value; };
  void set_windows(uint value) { windows_ = value; };
  void set_name(std::string const& value) { name_ = value; };
  bool contains_window(WindowAddress const& addr) const { return window_map_.contains(addr); }
  void insert_window(WindowCreationPayload create_window_paylod);
  std::string remove_window(WindowAddress const& addr);
  void initialize_window_map(const Json::Value& clients_data);

  bool on_window_opened(WindowCreationPayload const& create_window_paylod);
  std::optional<std::string> on_window_closed(WindowAddress const& addr);

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

  std::map<WindowAddress, std::string> window_map_;

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

  std::string get_rewrite(std::string window_class, std::string window_title);
  std::string& get_window_separator() { return format_window_separator_; }
  bool is_workspace_ignored(std::string& workspace_name);

  bool window_rewrite_config_uses_title() const { return any_window_rewrite_rule_uses_title_; }

 private:
  void onEvent(const std::string& e) override;
  void update_window_count();
  void sort_workspaces();
  void create_workspace(Json::Value const& workspace_data,
                        Json::Value const& clients_data = Json::Value::nullRef);
  void remove_workspace(std::string const& name);
  void set_urgent_workspace(std::string const& windowaddress);
  void parse_config(const Json::Value& config);
  void register_ipc();

  void on_window_opened(std::string const& payload);
  void on_window_closed(std::string const& payload);
  void on_window_moved(std::string const& payload);

  int window_rewrite_priority_function(std::string const& window_rule);

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
  util::RegexCollection window_rewrite_rules_;
  bool any_window_rewrite_rule_uses_title_ = false;
  std::string format_window_separator_;

  bool with_icon_;
  uint64_t monitor_id_;
  std::string active_workspace_name_;
  std::vector<std::unique_ptr<Workspace>> workspaces_;
  std::vector<Json::Value> workspaces_to_create_;
  std::vector<std::string> workspaces_to_remove_;
  std::vector<WindowCreationPayload> windows_to_create_;

  std::vector<std::regex> ignore_workspaces_;

  std::mutex mutex_;
  const Bar& bar_;
  Gtk::Box box_;
};

}  // namespace waybar::modules::hyprland
