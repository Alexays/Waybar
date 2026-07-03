#include "modules/mango/workspaces.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

namespace waybar::modules::mango {

Workspaces::Workspaces(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "workspaces", id, false, false), bar_(bar), box_(bar.orientation, 0) {
  box_.set_name("workspaces");
  if (!id.empty()) box_.get_style_context()->add_class(id);
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  if (config_["on-click"].isString()) on_click_left_ = config_["on-click"].asString();
  if (config_["on-click-middle"].isString())
    on_click_middle_ = config_["on-click-middle"].asString();
  if (config_["on-click-right"].isString()) on_click_right_ = config_["on-click-right"].asString();

  overview_button_ = new Gtk::Button("OVERVIEW");
  overview_button_->set_relief(Gtk::RELIEF_NONE);
  box_.pack_start(*overview_button_, false, false, 0);

  if (!on_click_left_.empty() || !on_click_middle_.empty() || !on_click_right_.empty()) {
    overview_button_->add_events(Gdk::BUTTON_PRESS_MASK);
    overview_button_->signal_button_press_event().connect(
        [this](GdkEventButton* event) -> bool { return handleButtonClick(event, 0, true); }, false);
  }

  IPC::getInstance().registerForIPC("monitor", this);
  dp.emit();
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
    btn.hide();
  }

  if (overview_mode) {
    overview_button_->show();
    auto style = overview_button_->get_style_context();
    style->add_class("overview");
    if (monitor["active"].asBool())
      style->add_class("current_output");
    else
      style->remove_class("current_output");

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
    overview_button_->hide();

    for (auto btn_it = buttons_.begin(); btn_it != buttons_.end();) {
      uint64_t id = btn_it->first;
      bool found = std::any_of(tags.begin(), tags.end(), [id](const Json::Value& tag) {
        return tag["index"].asUInt64() == id;
      });

      if (!found) {
        box_.remove(btn_it->second);
        btn_it = buttons_.erase(btn_it);
      } else {
        ++btn_it;
      }
    }

    for (const auto& tag : tags) {
      uint64_t idx = tag["index"].asUInt64();
      auto btn_it = buttons_.find(idx);
      Gtk::Button& button = (btn_it == buttons_.end()) ? addButton(idx) : btn_it->second;
      updateButtonState(button, tag, monitor);
    }

    std::vector<uint64_t> indices;
    for (const auto& tag : tags) indices.push_back(tag["index"].asUInt64());
    std::sort(indices.begin(), indices.end());
    int pos = 0;
    for (uint64_t idx : indices) {
      box_.reorder_child(buttons_[idx], pos + 1);
      pos++;
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
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);

  if (!on_click_left_.empty() || !on_click_middle_.empty() || !on_click_right_.empty()) {
    button.add_events(Gdk::BUTTON_PRESS_MASK);
    button.signal_button_press_event().connect(
        [this, idx](GdkEventButton* event) -> bool { return handleButtonClick(event, idx, false); },
        false);
  }

  button.show_all();
  return button;
}

void Workspaces::updateButtonState(Gtk::Button& button, const Json::Value& tag,
                                   const Json::Value& monitor) {
  auto style = button.get_style_context();
  bool active = tag["is_active"].asBool();
  bool urgent = tag["is_urgent"].asBool();
  bool empty = (tag["client_count"].asInt() == 0);

  if (active)
    style->add_class("active");
  else
    style->remove_class("active");

  if (urgent)
    style->add_class("urgent");
  else
    style->remove_class("urgent");

  if (empty)
    style->add_class("empty");
  else
    style->remove_class("empty");

  if (monitor["active"].asBool())
    style->add_class("current_output");
  else
    style->remove_class("current_output");

  uint64_t idx = tag["index"].asUInt64();
  std::string name = std::to_string(idx);
  if (config_["format"].isString()) {
    name = fmt::format(fmt::runtime(config_["format"].asString()),
                       fmt::arg("icon", getIcon(name, tag)), fmt::arg("value", name),
                       fmt::arg("index", idx), fmt::arg("output", monitor["name"].asString()));
  }

  if (!config_["disable-markup"].asBool()) {
    if (auto gtk_label = dynamic_cast<Gtk::Label*>(button.get_child())) {
      gtk_label->set_markup(name);
    }
  } else {
    button.set_label(name);
  }

  if (config_["current-only"].asBool()) {
    if (active)
      button.show();
    else
      button.hide();
  } else if (config_["hide-empty"].asBool() && empty && !active) {
    button.hide();
  } else {
    button.show();
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

bool Workspaces::handleButtonClick(GdkEventButton* event, uint64_t idx, bool isOverview) {
  std::string action;
  if (event->button == 1)
    action = on_click_left_;
  else if (event->button == 2)
    action = on_click_middle_;
  else if (event->button == 3)
    action = on_click_right_;

  if (action.empty()) return true;

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

  return true;
}

}  // namespace waybar::modules::mango