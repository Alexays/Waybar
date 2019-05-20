#include "modules/sway/workspaces.hpp"

namespace waybar::modules::sway {

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : bar_(bar),
      config_(config),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      scrolling_(false) {
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  ipc_.subscribe(R"(["workspace"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Workspaces::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Workspaces::onCmd));
  ipc_.sendCmd(IPC_GET_WORKSPACES);
  if (!config["disable-bar-scroll"].asBool()) {
    auto &window = const_cast<Bar &>(bar_).window;
    window.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    window.signal_scroll_event().connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }
  // Launch worker
  worker();
}

void Workspaces::onEvent(const struct Ipc::ipc_response &res) {
  try {
    ipc_.sendCmd(IPC_GET_WORKSPACES);
  } catch (const std::exception &e) {
    std::cerr << "Workspaces: " << e.what() << std::endl;
  }
}

void Workspaces::onCmd(const struct Ipc::ipc_response &res) {
  if (res.type == IPC_GET_WORKSPACES) {
    try {
      auto payload = parser_.parse(res.payload);
      if (payload.isArray()) {
        std::lock_guard<std::mutex> lock(mutex_);
        workspaces_.clear();
        std::copy_if(payload.begin(),
                     payload.end(),
                     std::back_inserter(workspaces_),
                     [&](const auto &workspace) {
                       return !config_["all-outputs"].asBool()
                                  ? workspace["output"].asString() == bar_.output->name
                                  : true;
                     });

        // adding persistant workspaces (as per the config file)
        const Json::Value &            p_workspaces = config_["persistant_workspaces"];
        const std::vector<std::string> p_workspaces_names = p_workspaces.getMemberNames();

        auto first_persistant_ws_idx = workspaces_.size();
        for (const std::string &p_w_name : p_workspaces_names) {
          const Json::Value &p_w = p_workspaces[p_w_name];
          auto               it =
              std::find_if(payload.begin(), payload.end(), [&p_w_name](const Json::Value &node) {
                return node["name"].asString() == p_w_name;
              });

          if (it != payload.end()) {
            continue;  // already displayed by some bar
          }

          if (p_w.isArray() && !p_w.empty()) {
            // Adding to target outputs
            for (const Json::Value &output : p_w) {
              if (output.asString() == bar_.output->name) {
                Json::Value v;
                v["name"] = p_w_name;
                v["target_output"] = bar_.output->name;
                workspaces_.emplace_back(std::move(v));
                break;
              }
            }
          } else {
            // Adding to all outputs
            Json::Value v;
            v["name"] = p_w_name;
            workspaces_.emplace_back(std::move(v));
          }
        }

        if (workspaces_order_.empty()) {
          // Saving starting order
          workspaces_order_.reserve(payload.size() + workspaces_.size() - first_persistant_ws_idx);
          for (const Json::Value &workspace : payload) {
            workspaces_order_.emplace_back(workspace["name"].asString());
          }
          for (auto i = first_persistant_ws_idx; i < workspaces_.size() ; ++i) {
            workspaces_order_.emplace_back(workspaces_[i]["name"].asString());
          }
        } else {
          // Ordering workspaces as it was before for current output
          std::vector<Json::Value> sorted_workspaces;
          sorted_workspaces.reserve(workspaces_.size());
          auto ws_end = workspaces_.end();
          for (const std::string &name_by_order : workspaces_order_) {
            auto it =
                std::find_if(workspaces_.begin(), ws_end, [&name_by_order](const Json::Value &ws) {
                  return ws["name"].asString() == name_by_order;
                });
            if (it != ws_end) {
              sorted_workspaces.emplace_back(*it);
              --ws_end;
              if (ws_end == workspaces_.begin()) {  // we've extracted everything
                break;
              }
              ws_end->swap(*it);
            }
          }

          // Adding new workspaces to the output (those were never showed in this output before)
          for (int i = 0; workspaces_.size() > sorted_workspaces.size(); ++i) {
            workspaces_order_.emplace_back(workspaces_[i]["name"].asString());
            sorted_workspaces.emplace_back(workspaces_[i]);
          }
          workspaces_.swap(sorted_workspaces);
        }

        dp.emit();
      }
    } catch (const std::exception &e) {
      std::cerr << "Workspaces: " << e.what() << std::endl;
    }
  } else {
    if (scrolling_) {
      scrolling_ = false;
    }
  }
}

void Workspaces::worker() {
  thread_ = [this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception &e) {
      std::cerr << "Workspaces: " << e.what() << std::endl;
    }
  };
}

bool Workspaces::filterButtons() {
  bool needReorder = false;
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto ws = std::find_if(workspaces_.begin(), workspaces_.end(), [it](const auto &node) {
      return node["name"].asString() == it->first;
    });
    if (ws == workspaces_.end() ||
        (!config_["all-outputs"].asBool() && (*ws)["output"].asString() != bar_.output->name)) {
      it = buttons_.erase(it);
      needReorder = true;
    } else {
      ++it;
    }
  }
  return needReorder;
}

auto Workspaces::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  bool                        needReorder = filterButtons();
  for (auto it = workspaces_.begin(); it != workspaces_.end(); ++it) {
    auto bit = buttons_.find((*it)["name"].asString());
    if (bit == buttons_.end()) {
      needReorder = true;
    }
    auto &button = bit == buttons_.end() ? addButton(*it) : bit->second;
    if ((*it)["focused"].asBool()) {
      button.get_style_context()->add_class("focused");
    } else {
      button.get_style_context()->remove_class("focused");
    }
    if ((*it)["visible"].asBool()) {
      button.get_style_context()->add_class("visible");
    } else {
      button.get_style_context()->remove_class("visible");
    }
    if ((*it)["urgent"].asBool()) {
      button.get_style_context()->add_class("urgent");
    } else {
      button.get_style_context()->remove_class("urgent");
    }
    if (needReorder) {
      box_.reorder_child(button, it - workspaces_.begin());
    }
    std::string output = getIcon((*it)["name"].asString(), *it);
    if (config_["format"].isString()) {
      auto format = config_["format"].asString();
      output = fmt::format(format,
                           fmt::arg("icon", output),
                           fmt::arg("name", trimWorkspaceName((*it)["name"].asString())),
                           fmt::arg("index", (*it)["num"].asString()));
    }
    if (!config_["disable-markup"].asBool()) {
      static_cast<Gtk::Label *>(button.get_children()[0])->set_markup(output);
    } else {
      button.set_label(output);
    }
    onButtonReady(*it, button);
  }
}

Gtk::Button &Workspaces::addButton(const Json::Value &node) {
  auto  pair = buttons_.emplace(node["name"].asString(), node["name"].asString());
  auto &button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_relief(Gtk::RELIEF_NONE);
  button.signal_clicked().connect([this, node] {
    try {
      if (node["target_output"].isString()) {
        ipc_.sendCmd(
            IPC_COMMAND,
            fmt::format("workspace \"{}\"; move workspace to output \"{}\"; workspace \"{}\"",
                        node["name"].asString(),
                        node["target_output"].asString(),
                        node["name"].asString()));
      } else {
        ipc_.sendCmd(IPC_COMMAND, fmt::format("workspace \"{}\"", node["name"].asString()));
      }
    } catch (const std::exception &e) {
      std::cerr << e.what() << std::endl;
    }
  });
  if (!config_["disable-scroll"].asBool()) {
    button.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    button.signal_scroll_event().connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }
  return button;
}

std::string Workspaces::getIcon(const std::string &name, const Json::Value &node) {
  std::vector<std::string> keys = {name, "urgent", "focused", "visible", "default"};
  for (auto const &key : keys) {
    if (key == "focused" || key == "visible" || key == "urgent") {
      if (config_["format-icons"][key].isString() && node[key].asBool()) {
        return config_["format-icons"][key].asString();
      }
    } else if (config_["format-icons"][key].isString()) {
      return config_["format-icons"][key].asString();
    }
  }
  return name;
}

bool Workspaces::handleScroll(GdkEventScroll *e) {
  // Avoid concurrent scroll event
  if (scrolling_) {
    return false;
  }
  std::string name;
  scrolling_ = true;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(workspaces_.begin(), workspaces_.end(), [](const auto &workspace) {
      return workspace["focused"].asBool();
    });
    if (it == workspaces_.end()) {
      scrolling_ = false;
      return false;
    }
    switch (e->direction) {
      case GDK_SCROLL_DOWN:
      case GDK_SCROLL_RIGHT:
        name = getCycleWorkspace(it, false);
        break;
      case GDK_SCROLL_UP:
      case GDK_SCROLL_LEFT:
        name = getCycleWorkspace(it, true);
        break;
      case GDK_SCROLL_SMOOTH:
        gdouble delta_x, delta_y;
        gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent *>(e), &delta_x, &delta_y);
        if (delta_y < 0) {
          name = getCycleWorkspace(it, true);
        } else if (delta_y > 0) {
          name = getCycleWorkspace(it, false);
        }
        break;
      default:
        break;
    }
    if (name.empty() || name == (*it)["name"].asString()) {
      scrolling_ = false;
      return false;
    }
  }
  try {
    ipc_.sendCmd(IPC_COMMAND, fmt::format("workspace \"{}\"", name));
  } catch (const std::exception &e) {
    std::cerr << "Workspaces: " << e.what() << std::endl;
  }
  return true;
}

const std::string Workspaces::getCycleWorkspace(std::vector<Json::Value>::iterator it,
                                                bool                               prev) const {
  if (prev && it == workspaces_.begin() && !config_["disable-scroll-wraparound"].asBool()) {
    return (*(--workspaces_.end()))["name"].asString();
  }
  if (prev && it != workspaces_.begin())
    --it;
  else if (!prev && it != workspaces_.end())
    ++it;
  if (!prev && it == workspaces_.end()) {
    if (config_["disable-scroll-wraparound"].asBool()) {
      --it;
    } else {
      return (*(workspaces_.begin()))["name"].asString();
    }
  }
  return (*it)["name"].asString();
}

std::string Workspaces::trimWorkspaceName(std::string name) {
  std::size_t found = name.find(':');
  if (found != std::string::npos) {
    return name.substr(found + 1);
  }
  return name;
}

void Workspaces::onButtonReady(const Json::Value &node, Gtk::Button &button) {
  if (config_["current-only"].asBool()) {
    if (node["focused"].asBool()) {
      button.show();
    } else {
      button.hide();
    }
  } else {
    button.show();
  }
}

Workspaces::operator Gtk::Widget &() { return box_; }

}  // namespace waybar::modules::sway
