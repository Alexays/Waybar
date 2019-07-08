#pragma once

#include <glibmm/markup.h>
#include <gtkmm/label.h>
#include <json/json.h>
#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule {
 public:
  ALabel(const Json::Value &, const std::string &, const std::string &, const std::string &format,
         uint16_t interval = 0, bool ellipsize = false);
  virtual ~ALabel() = default;
  virtual auto        update() -> void;
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0);

 protected:
  Gtk::Label                 label_;
  std::string                format_;
  std::string                click_param;
  const std::chrono::seconds interval_;
  bool                       alt_ = false;
  std::string                default_format_;

  virtual bool        handleToggle(GdkEventButton *const &e);
  virtual std::string getState(uint8_t value, bool lesser = false);

  struct Arg {
    std::function<Json::Value(void)> func;
    bool                             isState = false;
    bool                             reversedState = false;
    bool                             isDefault = false;
    bool                             tooltip = false;
  };

  std::string old_state_;

  std::tuple<Json::Value, const std::string> handleArg(const std::string &         format,
                                                       const std::string &         key,
                                                       std::pair<std::string, Arg> arg);
  const std::string                          extractArgs(const std::string &format);

  std::unordered_map<std::string, Arg> args_;
};

}  // namespace waybar
