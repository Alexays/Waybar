#pragma once

#include <fmt/format.h>
#include <glibmm/markup.h>
#include <gtkmm/label.h>
#include <json/json.h>

#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule {
 public:
  using args = fmt::dynamic_format_arg_store<fmt::format_context>;

  ALabel(const Json::Value &config,
         const std::string &name,
         const std::string &id,
         const std::string &format,
         const std::string &formatTooltip,
         uint16_t interval = 0,
         bool ellipsize = false);
  virtual ~ALabel() = default;
  virtual auto update() -> void override;
  virtual auto update(const std::string format, args &args) -> void;
  virtual std::string getIcon(uint16_t percentage, const std::string &alt = "", uint16_t max = 0);

 protected:
  Gtk::Label label_;
  std::string format_;
  std::string click_param;
  const std::chrono::seconds interval_;
  bool alt_ = false;

  virtual bool hasFormat(const std::string &key) const;
  virtual const std::string &getFormat() const;
  virtual bool handleToggle(GdkEventButton *const &e);
  virtual std::string getState(uint16_t value, bool lesser = false);
};

}  // namespace waybar
