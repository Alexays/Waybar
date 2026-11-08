#include "modules/mango/workspaces.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace waybar::modules::mango {

Workspaces::Workspaces(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "workspaces", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) box_.add_css_class(id);
  box_.add_css_class(MODULE_CLASS);
  event_box_.set_child(box_);

  if (config_["on-click"].isString()) on_click_left_ = config_["on-click"].asString();
  if (config_["on-click-middle"].isString())
    on_click_middle_ = config_["on-click-middle"].asString();
  if (config_["on-click-right"].isString()) on_click_right_ = config_["on-click-right"].asString();

  const auto& tag_filter_cfg = config_["tag-filter"];
  if (tag_filter_cfg.isArray()) {
    for (const auto& tag : tag_filter_cfg) {
      if (tag.isUInt64()) {
        tag_filter_.insert(tag.asUInt64());
      }
    }
  } else if (tag_filter_cfg.isObject()) {
    const auto& output_name = bar_.output->name;
    if (tag_filter_cfg.isMember(output_name) && tag_filter_cfg[output_name].isArray()) {
      for (const auto& tag : tag_filter_cfg[output_name]) {
        if (tag.isUInt64()) {
          tag_filter_.insert(tag.asUInt64());
        }
      }
    }
  }

  overview_button_ = new Gtk::Button("OVERVIEW");
  overview_button_->set_has_frame(false);
  box_.append(*overview_button_);
  addClickHandler(*overview_button_, 0, true);

  IPC::getInstance().registerForIPC("monitor", this);
}

Workspaces::~Workspaces() {
  IPC::getInstance().unregisterForIPC(this);

  if (overview_button_) {
    box_.remove(*overview_button_);
    delete overview_button_;
    overview_button_ = nullptr;
  }

  for (auto& [idx, btn] : buttons_) {
    box_.remove(btn);
  }
  buttons_.clear();
}

bool Workspaces::isTagVisible(uint64_t idx) const {
  return tag_filter_.empty() || tag_filter_.contains(idx);
}

void Workspaces::onEvent(const Json::Value& ev) { dp.emit(); }

void Workspaces::doUpdate() {
  Json::Value monitor = IPC::getInstance().getMonitor(bar_.output->name);
  if (monitor.isNull()) return;

  const auto& tags = monitor["tags"];

  bool overview_mode = false;
  if (monitor.isMember("active_tags") && monitor["active_tags"].isArray()) {
    const auto& active_tags = monitor["active_tags"];
    if (active_tags.size() == 1 && active_tags[0].asInt() == 0) {
      overview_mode = true;
    }
  }

  for (auto& [idx, btn] : buttons_) {
    btn.set_visible(false);
  }

  if (overview_mode) {
    overview_button_->set_visible(true);
    overview_button_->add_css_class("overview");
    if (monitor["active"].asBool())
      overview_button_->add_css_class("current_output");
    else
      overview_button_->remove_css_class("current_output");

    std::string label =
        config_["overview-label"].isString() ? config_["overview-label"].asString() : "OVERVIEW";

    if (!config_["disable-markup"].asBool()) {
      if (auto gtk_label = dynamic_cast<Gtk::Label*>(overview_button_->get_child())) {
        gtk_label->set_markup(label);
      }
    } else {
      overview_button_->set_label(label);
    }
  } else {
    overview_button_->set_visible(false);

    for (auto btn_it = buttons_.begin(); btn_it != buttons_.end();) {
      uint64_t id = btn_it->first;

      bool found = std::any_of(tags.begin(), tags.end(), [id](const Json::Value& tag) {
        return tag["index"].asUInt64() == id;
      });

      if (!isTagVisible(id) || !found) {
        box_.remove(btn_it->second);
        btn_it = buttons_.erase(btn_it);
      } else {
        ++btn_it;
      }
    }

    for (const auto& tag : tags) {
      uint64_t idx = tag["index"].asUInt64();

      if (!isTagVisible(idx)) {
        continue;
      }

      auto btn_it = buttons_.find(idx);
      Gtk::Button& button = (btn_it == buttons_.end()) ? addButton(idx) : btn_it->second;
      updateButtonState(button, tag, monitor);
    }

    std::vector<uint64_t> indices;
    for (const auto& tag : tags) {
      uint64_t idx = tag["index"].asUInt64();

      if (!isTagVisible(idx)) {
        continue;
      }

      indices.push_back(idx);
    }

    std::sort(indices.begin(), indices.end());

    Gtk::Widget* prev = overview_button_;
    for (uint64_t idx : indices) {
      Gtk::Button& button = buttons_[idx];
      box_.reorder_child_after(button, *prev);
      prev = &button;
    }
  }
}

void Workspaces::update() {
  doUpdate();
  AModule::update();
}

Gtk::Button& Workspaces::addButton(uint64_t idx) {
  auto [it, _] = buttons_.emplace(idx, std::to_string(idx));
  auto& button = it->second;
  box_.append(button);
  button.set_has_frame(false);
  addClickHandler(button, idx, false);
  return button;
}

void Workspaces::addClickHandler(Gtk::Button& button, uint64_t idx, bool isOverview) {
  if (on_click_left_.empty() && on_click_middle_.empty() && on_click_right_.empty()) {
    return;
  }
  // Capture phase: every press is claimed before the button's own click gesture sees it
  auto click = Gtk::GestureClick::create();
  click->set_button(0);
  click->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
  click->signal_pressed().connect(
      [this, click = click.get(), idx, isOverview](int n_press, double, double) {
        const guint button = click->get_current_button();
        handleButtonClick(button, idx, isOverview);
        // Like AModule, a double (triple) click is a press followed by a second event
        if ((n_press - 1) % 3 != 0) {
          handleButtonClick(button, idx, isOverview);
        }
        click->set_state(Gtk::EventSequenceState::CLAIMED);
      });
  button.add_controller(click);
}

void Workspaces::updateButtonState(Gtk::Button& button, const Json::Value& tag,
                                   const Json::Value& monitor) {
  bool active = tag["is_active"].asBool();
  bool urgent = tag["is_urgent"].asBool();
  bool empty = (tag["client_count"].asInt() == 0);

  if (active)
    button.add_css_class("active");
  else
    button.remove_css_class("active");

  if (urgent)
    button.add_css_class("urgent");
  else
    button.remove_css_class("urgent");

  if (empty)
    button.add_css_class("empty");
  else
    button.remove_css_class("empty");

  if (monitor["active"].asBool())
    button.add_css_class("current_output");
  else
    button.remove_css_class("current_output");

  uint64_t idx = tag["index"].asUInt64();
  std::string name = std::to_string(idx);
  if (config_["format"].isString()) {
    name = fmt::format(fmt::runtime(config_["format"].asString()),
                       fmt::arg("icon", getIcon(name, tag)), fmt::arg("value", name),
                       fmt::arg("name", name), fmt::arg("index", idx),
                       fmt::arg("output", monitor["name"].asString()));
  }

  if (!config_["disable-markup"].asBool()) {
    if (auto gtk_label = dynamic_cast<Gtk::Label*>(button.get_child())) {
      gtk_label->set_markup(name);
    }
  } else {
    button.set_label(name);
  }

  if (config_["current-only"].asBool()) {
    button.set_visible(active);
  } else {
    button.set_visible(!(config_["hide-empty"].asBool() && empty && !active));
  }
}

std::string Workspaces::getIcon(const std::string& value, const Json::Value& tag) {
  const auto& icons = config_["format-icons"];
  if (!icons) return value;

  if (tag["is_urgent"].asBool() && icons["urgent"]) return icons["urgent"].asString();
  if (tag["is_active"].asBool() && icons["active"]) return icons["active"].asString();
  if (tag["client_count"].asInt() == 0 && icons["empty"]) return icons["empty"].asString();

  std::string idx = std::to_string(tag["index"].asUInt());
  if (icons[idx]) return icons[idx].asString();
  if (icons["default"]) return icons["default"].asString();
  return value;
}

void Workspaces::handleButtonClick(guint button, uint64_t idx, bool isOverview) {
  std::string action;
  if (button == 1)
    action = on_click_left_;
  else if (button == 2)
    action = on_click_middle_;
  else if (button == 3)
    action = on_click_right_;

  if (action.empty()) return;

  try {
    std::string cmd;
    if (isOverview) {
      if (action == "activate")
        cmd = "dispatch overview";
      else if (action == "toggle")
        cmd = "dispatch toggleoverview";
    } else {
      if (action == "activate")
        cmd = "dispatch view," + std::to_string(idx);
      else if (action == "toggle")
        cmd = "dispatch toggleview," + std::to_string(idx);
    }

    if (!cmd.empty()) {
      Json::Value req;
      req["command"] = cmd;
      IPC::sendAsync(req);
    }
  } catch (const std::exception& e) {
    spdlog::error("Error sending IPC command: {}", e.what());
  }
}

}  // namespace waybar::modules::mango
