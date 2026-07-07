#pragma once

#include <fmt/args.h>
#include <fmt/format.h>
#include <glibmm/markup.h>
#include <gtkmm/label.h>
#include <json/json.h>

#include <optional>
#include <string>
#include <utility>

#include "AModule.hpp"

namespace waybar {

class ALabel : public AModule {
 public:
  ALabel(const Json::Value&, const std::string&, const std::string&, const std::string& format,
         std::mutex& reap_mtx, std::list<pid_t>& reap, uint16_t interval = 0,
         bool ellipsize = false, bool enable_click = false, bool enable_scroll = false);
  virtual ~ALabel() = default;
  auto update() -> void override;
  virtual std::string getIcon(uint16_t, const std::string& alt = "", uint16_t max = 0);
  virtual std::string getIcon(uint16_t, const std::vector<std::string>& alts, uint16_t max = 0);

 protected:
  Gtk::Label label_;
  std::string format_;
  const std::chrono::milliseconds interval_;
  bool alt_ = false;
  std::string default_format_;

  bool setLabelMarkup(const Glib::ustring& markup);
  bool setTooltipMarkup(const Glib::ustring& markup);

  // resolveTooltipFormat() / resolveFormat() are inherited from AModule.

  // Combined label + tooltip helper. Builds a single fmt argument store from
  // `args`, renders `labelFormat` into the label and the resolved tooltip format
  // into the tooltip, both through the dedup-aware setters. Honors the `tooltip`
  // toggle. This replaces the label/tooltip formatting boilerplate that modules
  // used to duplicate. `state` selects `tooltip-format-<state>` when non-empty.
  template <typename... Args>
  void updateLabelAndTooltipForState(const std::string& state, const std::string& labelFormat,
                                     const std::string& tooltipDefault, Args&&... args) {
    fmt::dynamic_format_arg_store<fmt::format_context> store;
    (store.push_back(std::forward<Args>(args)), ...);
    setLabelMarkup(fmt::vformat(labelFormat, store));
    if (tooltipEnabled()) {
      setTooltipMarkup(fmt::vformat(resolveTooltipFormat(tooltipDefault, state), store));
    }
  }

  template <typename... Args>
  void updateLabelAndTooltip(const std::string& labelFormat, const std::string& tooltipDefault,
                             Args&&... args) {
    updateLabelAndTooltipForState("", labelFormat, tooltipDefault, std::forward<Args>(args)...);
  }

  // Overloads accepting a pre-built argument store, for modules that must
  // assemble a dynamic set of format arguments (e.g. per-core CPU stats) that
  // cannot be expressed through a fixed variadic call.
  // A non-const reference is used so this overload is preferred over the
  // variadic template above (which would otherwise bind the store as a single
  // forwarded argument).
  void updateLabelAndTooltipForState(const std::string& state, const std::string& labelFormat,
                                     const std::string& tooltipDefault,
                                     fmt::dynamic_format_arg_store<fmt::format_context>& store) {
    setLabelMarkup(fmt::vformat(labelFormat, store));
    if (tooltipEnabled()) {
      setTooltipMarkup(fmt::vformat(resolveTooltipFormat(tooltipDefault, state), store));
    }
  }

  void updateLabelAndTooltip(const std::string& labelFormat, const std::string& tooltipDefault,
                             fmt::dynamic_format_arg_store<fmt::format_context>& store) {
    updateLabelAndTooltipForState("", labelFormat, tooltipDefault, store);
  }

  bool handleToggle(GdkEventButton* const& e) override;
  void copyToClipboard(const std::string&);
  virtual std::string getState(uint8_t value, bool lesser = false);

  std::map<std::string, GtkMenuItem*> submenus_;
  std::map<std::string, std::string> menuActionsMap_;
  static void handleGtkMenuEvent(GtkMenuItem* menuitem, gpointer data);

 private:
  // Raw UTF-8 bytes, not Glib::ustring: ustring::operator== collates with
  // g_utf8_collate(), which gives private-use codepoints (nerd-font icons)
  // no collation weight, so two different icons compare equal.
  std::optional<std::string> last_label_markup_;
  std::optional<std::string> last_tooltip_markup_;
};

}  // namespace waybar
