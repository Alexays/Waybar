#pragma once

#include <fmt/core.h>
#include <fmt/format.h>
#include <glibmm/markup.h>
#include <gtkmm/label.h>
#include <json/json.h>

#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule {
 public:
  ALabel(const Json::Value &config,
         const std::string &name,
         const std::string &id,
         const std::string &format,
         const std::string &formatTooltip,
         uint16_t interval = 0,
         bool ellipsize = false);
  virtual ~ALabel() = default;
  virtual auto update() -> void override;
  virtual auto update(const std::string format,
                      fmt::dynamic_format_arg_store<fmt::format_context> &args) -> void;
  virtual auto update(const std::string format,
                      fmt::dynamic_format_arg_store<fmt::format_context> &args,
                      std::string tooltipFormat) -> void;
  virtual std::string getIcon(uint16_t percentage, const std::string &alt = "", uint16_t max = 0);
  virtual const std::string &getFormat() const;
  virtual const std::string getFormat(const std::string &prefix,
                                      const std::string &a,
                                      const std::string &b = "");

 protected:
  Gtk::Label label_;
  std::string format_;
  std::string click_param;
  const std::chrono::seconds interval_;
  bool alt_ = false;

  virtual bool hasFormat(const std::string &key) const;
  virtual bool hasFormat(const std::string &key, const std::string &format) const;
  virtual bool handleToggle(GdkEventButton *const &e) override;
  virtual std::string getState(uint16_t value, bool lesser = false);
};

}  // namespace waybar
