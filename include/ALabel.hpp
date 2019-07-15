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
  virtual std::string getIcon(uint16_t, const std::string &alt = "", uint16_t max = 0) const;

 protected:
  Gtk::Label                 label_;
  std::string                format_;
  std::string                click_param;
  const std::chrono::seconds interval_;
  bool                       alt_ = false;
  std::string                default_format_;

  virtual bool                           handleToggle(GdkEventButton *const &e);
  virtual const std::string              getState(uint8_t value, bool lesser = false) const;
  virtual const std::string              getFormat() const;
  virtual const std::vector<std::string> getClasses() const;

  enum STATE_TYPE {
    NONE = 0,
    STATE = 1 << 0,
    REVERSED_STATE = 1 << 1,
    STATE_ALT = 1 << 2,
    TOOLTIP = 1 << 3,
    DEFAULT = 1 << 4
  };
  friend STATE_TYPE operator|(STATE_TYPE a, STATE_TYPE b) {
    typedef std::underlying_type<STATE_TYPE>::type UL;
    return ALabel::STATE_TYPE(static_cast<UL>(a) | static_cast<UL>(b));
  };

  struct Arg {
    const std::string                key;
    std::function<Json::Value(void)> func;
    STATE_TYPE                       state = STATE_TYPE::NONE;
    std::size_t                      state_threshold = 0;
  };

  std::tuple<const Json::Value, const std::string> handleArg(const std::string &format,
                                                             const std::string &key,
                                                             const Arg &        arg) const;
  const std::string                                extractArgs(const std::string &format);

  std::vector<Arg> args_;

 private:
  bool checkFormatArg(const std::string &format, const Arg &arg);
};

}  // namespace waybar
