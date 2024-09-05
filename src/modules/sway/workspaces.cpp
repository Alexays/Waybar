#include "modules/sway/workspaces.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <string>

namespace waybar::modules::sway {

// Helper function to assign a number to a workspace, just like sway. In fact
// this is taken quite verbatim from `sway/ipc-json.c`.
int Workspaces::convertWorkspaceNameToNum(std::string name) {
  if (isdigit(name[0]) != 0) {
    errno = 0;
    char *endptr = nullptr;
    long long parsed_num = strtoll(name.c_str(), &endptr, 10);
    if (errno != 0 || parsed_num > INT32_MAX || parsed_num < 0 || endptr == name.c_str()) {
      return -1;
    }
    return (int)parsed_num;
  }
  return -1;
}

int Workspaces::windowRewritePriorityFunction(std::string const &window_rule) {
  // Rules that match against title are prioritized
  // Rules that don't specify if they're matching against either title or class are deprioritized
  bool const hasTitle = window_rule.find("title") != std::string::npos;
  bool const hasClass = window_rule.find("class") != std::string::npos;

  if (hasTitle && hasClass) {
    return 3;
  }
  if (hasTitle) {
    return 2;
  }
  if (hasClass) {
    return 1;
  }
  return 0;
}

Workspaces::Workspaces(const std::string &id, const Bar &bar, const Json::Value &config)
    : AModule(config, "workspaces", id, false, !config["disable-scroll"].asBool()),
      bar_(bar),
      box_(bar.orientation, 0) {
  if (config["format-icons"]["high-priority-named"].isArray()) {
    for (const auto &it : config["format-icons"]["high-priority-named"]) {
      high_priority_named_.push_back(it.asString());
    }
  }
  box_.set_name("workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);
  if (config_["format-window-separator"].isString()) {
    m_formatWindowSeperator = config_["format-window-separator"].asString();
  } else {
    m_formatWindowSeperator = " ";
  }
  const Json::Value &windowRewrite = config["window-rewrite"];

  const Json::Value &windowRewriteDefaultConfig = config["window-rewrite-default"];
  m_windowRewriteDefault =
      windowRewriteDefaultConfig.isString() ? windowRewriteDefaultConfig.asString() : "?";

  m_windowRewriteRules = waybar::util::RegexCollection(
      windowRewrite, m_windowRewriteDefault,
      [](std::string &window_rule) { return windowRewritePriorityFunction(window_rule); });
  ipc_.subscribe(R"(["workspace"])");
  ipc_.subscribe(R"(["window"])");
  ipc_.signal_event.connect(sigc::mem_fun(*this, &Workspaces::onEvent));
  ipc_.signal_cmd.connect(sigc::mem_fun(*this, &Workspaces::onCmd));
  ipc_.sendCmd(IPC_GET_TREE);
  if (config["enable-bar-scroll"].asBool()) {
    auto &window = const_cast<Bar &>(bar_).window;
    window.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    window.signal_scroll_event().connect(sigc::mem_fun(*this, &Workspaces::handleScroll));
  }
  // Launch worker
  ipc_.setWorker([this] {
    try {
      ipc_.handleEvent();
    } catch (const std::exception &e) {
      spdlog::error("Workspaces: {}", e.what());
    }
  });
}

void Workspaces::onEvent(const struct Ipc::ipc_response &res) {
  try {
    ipc_.sendCmd(IPC_GET_TREE);
  } catch (const std::exception &e) {
    spdlog::error("Workspaces: {}", e.what());
  }
}

void Workspaces::onCmd(const struct Ipc::ipc_response &res) {
  if (res.type == IPC_GET_TREE) {
    try {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto payload = parser_.parse(res.payload);
        workspaces_.clear();
        std::vector<Json::Value> outputs;
        bool alloutputs = config_["all-outputs"].asBool();
        std::copy_if(payload["nodes"].begin(), payload["nodes"].end(), std::back_inserter(outputs),
                     [&](const auto &output) {
                       if (alloutputs && output["name"].asString() != "__i3") {
                         return true;
                       }
                       if (output["name"].asString() == bar_.output->name) {
                         return true;
                       }
                       return false;
                     });

        for (auto &output : outputs) {
          std::copy(output["nodes"].begin(), output["nodes"].end(),
                    std::back_inserter(workspaces_));
          std::copy(output["floating_nodes"].begin(), output["floating_nodes"].end(),
                    std::back_inserter(workspaces_));
        }

        // adding persistent workspaces (as per the config file)
        if (config_["persistent-workspaces"].isObject()) {
          const Json::Value &p_workspaces = config_["persistent-workspaces"];
          const std::vector<std::string> p_workspaces_names = p_workspaces.getMemberNames();

          for (const std::string &p_w_name : p_workspaces_names) {
            const Json::Value &p_w = p_workspaces[p_w_name];
            auto it = std::find_if(workspaces_.begin(), workspaces_.end(),
                                   [&p_w_name](const Json::Value &node) {
                                     return node["name"].asString() == p_w_name;
                                   });

            if (it != workspaces_.end()) {
              continue;  // already displayed by some bar
            }

            if (p_w.isArray() && !p_w.empty()) {
              // Adding to target outputs
              for (const Json::Value &output : p_w) {
                if (output.asString() == bar_.output->name) {
                  Json::Value v;
                  v["name"] = p_w_name;
                  v["target_output"] = bar_.output->name;
                  v["num"] = convertWorkspaceNameToNum(p_w_name);
                  workspaces_.emplace_back(std::move(v));
                  break;
                }
              }
            } else {
              // Adding to all outputs
              Json::Value v;
              v["name"] = p_w_name;
              v["target_output"] = "";
              v["num"] = convertWorkspaceNameToNum(p_w_name);
              workspaces_.emplace_back(std::move(v));
            }
          }
        }

        // sway has a defined ordering of workspaces that should be preserved in
        // the representation displayed by waybar to ensure that commands such
        // as "workspace prev" or "workspace next" make sense when looking at
        // the workspace representation in the bar.
        // Due to waybar's own feature of persistent workspaces unknown to sway,
        // custom sorting logic is necessary to make these workspaces appear
        // naturally in the list of workspaces without messing up sway's
        // sorting. For this purpose, a custom numbering property is created
        // that preserves the order provided by sway while inserting numbered
        // persistent workspaces at their natural positions.
        //
        // All of this code assumes that sway provides numbered workspaces first
        // and other workspaces are sorted by their creation time.
        //
        // In a first pass, the maximum "num" value is computed to enqueue
        // unnumbered workspaces behind numbered ones when computing the sort
        // attribute.
        //
        // Note: if the 'alphabetical_sort' option is true, the user is in
        // agreement that the "workspace prev/next" commands may not follow
        // the order displayed in Waybar.
        int max_num = -1;
        for (auto &workspace : workspaces_) {
          max_num = std::max(workspace["num"].asInt(), max_num);
        }
        for (auto &workspace : workspaces_) {
          auto workspace_num = workspace["num"].asInt();
          if (workspace_num > -1) {
            workspace["sort"] = workspace_num;
          } else {
            workspace["sort"] = ++max_num;
          }
        }
        std::sort(workspaces_.begin(), workspaces_.end(),
                  [this](const Json::Value &lhs, const Json::Value &rhs) {
                    auto lname = lhs["name"].asString();
                    auto rname = rhs["name"].asString();
                    int l = lhs["sort"].asInt();
                    int r = rhs["sort"].asInt();

                    if (l == r || config_["alphabetical_sort"].asBool()) {
                      // In case both integers are the same, lexicographical
                      // sort. The code above already ensure that this will only
                      // happened in case of explicitly numbered workspaces.
                      //
                      // Additionally, if the config specifies to sort workspaces
                      // alphabetically do this here.
                      return lname < rname;
                    }

                    return l < r;
                  });
      }
      dp.emit();
    } catch (const std::exception &e) {
      spdlog::error("Workspaces: {}", e.what());
    }
  }
}

bool Workspaces::filterButtons() {
  bool needReorder = false;
  for (auto it = buttons_.begin(); it != buttons_.end();) {
    auto ws = std::find_if(workspaces_.begin(), workspaces_.end(),
                           [it](const auto &node) { return node["name"].asString() == it->first; });
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

bool Workspaces::hasFlag(const Json::Value &node, const std::string &flag) {
  if (node[flag].asBool()) {
    return true;
  }

  if (std::any_of(node["nodes"].begin(), node["nodes"].end(),
                  [&](auto const &e) { return hasFlag(e, flag); })) {
    return true;
  }
  if (std::any_of(node["floating_nodes"].begin(), node["floating_nodes"].end(),
                  [&](auto const &e) { return hasFlag(e, flag); })) {
    return true;
  }
  return false;
}

void Workspaces::updateWindows(const Json::Value &node, std::string &windows) {
  if ((node["type"].asString() == "con" || node["type"].asString() == "floating_con") &&
      node["name"].isString()) {
    std::string title = g_markup_escape_text(node["name"].asString().c_str(), -1);
    std::string windowClass = node["app_id"].asString();

    // Only add window rewrites that can be looked up
    if (!windowClass.empty()) {
      std::string windowReprKey = fmt::format("class<{}> title<{}>", windowClass, title);
      std::string window = m_windowRewriteRules.get(windowReprKey);
      // allow result to have formatting
      window = fmt::format(fmt::runtime(window), fmt::arg("name", title),
                           fmt::arg("class", windowClass));
      windows.append(window);
      windows.append(m_formatWindowSeperator);
    }
  }
  for (const Json::Value &child : node["nodes"]) {
    updateWindows(child, windows);
  }
  for (const Json::Value &child : node["floating_nodes"]) {
    updateWindows(child, windows);
  }
}

auto Workspaces::update() -> void {
  std::lock_guard<std::mutex> lock(mutex_);
  bool needReorder = filterButtons();
  for (auto it = workspaces_.begin(); it != workspaces_.end(); ++it) {
    auto bit = buttons_.find((*it)["name"].asString());
    if (bit == buttons_.end()) {
      needReorder = true;
    }
    auto &button = bit == buttons_.end() ? addButton(*it) : bit->second;
    if (needReorder) {
      box_.reorder_child(button, it - workspaces_.begin());
    }
    bool noNodes = (*it)["nodes"].empty() && (*it)["floating_nodes"].empty();
    if (hasFlag((*it), "focused")) {
      button.get_style_context()->add_class("focused");
    } else {
      button.get_style_context()->remove_class("focused");
    }
    if (hasFlag((*it), "visible") || ((*it)["output"].isString() && noNodes)) {
      button.get_style_context()->add_class("visible");
    } else {
      button.get_style_context()->remove_class("visible");
    }
    if (hasFlag((*it), "urgent")) {
      button.get_style_context()->add_class("urgent");
    } else {
      button.get_style_context()->remove_class("urgent");
    }
    if ((*it)["target_output"].isString()) {
      button.get_style_context()->add_class("persistent");
    } else {
      button.get_style_context()->remove_class("persistent");
    }
    if (noNodes) {
      button.get_style_context()->add_class("empty");
    } else {
      button.get_style_context()->remove_class("empty");
    }
    if ((*it)["output"].isString()) {
      if (((*it)["output"].asString()) == bar_.output->name) {
        button.get_style_context()->add_class("current_output");
      } else {
        button.get_style_context()->remove_class("current_output");
      }
    } else {
      button.get_style_context()->remove_class("current_output");
    }
    std::string output = (*it)["name"].asString();
    std::string windows = "";
    if (config_["window-format"].isString()) {
      updateWindows((*it), windows);
    }
    if (config_["format"].isString()) {
      auto format = config_["format"].asString();
      output = fmt::format(
          fmt::runtime(format), fmt::arg("icon", getIcon(output, *it)), fmt::arg("value", output),
          fmt::arg("name", trimWorkspaceName(output)), fmt::arg("index", (*it)["num"].asString()),
          fmt::arg("windows",
                   windows.substr(0, windows.length() - m_formatWindowSeperator.length())),
          fmt::arg("output", (*it)["output"].asString()));
    }
    if (!config_["disable-markup"].asBool()) {
      static_cast<Gtk::Label *>(button.get_children()[0])->set_markup(output);
    } else {
      button.set_label(output);
    }
    onButtonReady(*it, button);
  }
  // Call parent update
  AModule::update();
}

Gtk::Button &Workspaces::addButton(const Json::Value &node) {
  auto pair = buttons_.emplace(node["name"].asString(), node["name"].asString());
  auto &&button = pair.first->second;
  box_.pack_start(button, false, false, 0);
  button.set_name("sway-workspace-" + node["name"].asString());
  button.set_relief(Gtk::RELIEF_NONE);
  if (!config_["disable-click"].asBool()) {
    button.signal_pressed().connect([this, node] {
      try {
        if (node["target_output"].isString()) {
          ipc_.sendCmd(IPC_COMMAND,
                       fmt::format(persistent_workspace_switch_cmd_, "--no-auto-back-and-forth",
                                   node["name"].asString(), node["target_output"].asString(),
                                   "--no-auto-back-and-forth", node["name"].asString()));
        } else {
          ipc_.sendCmd(IPC_COMMAND, fmt::format("workspace {} \"{}\"",
                                                config_["disable-auto-back-and-forth"].asBool()
                                                    ? "--no-auto-back-and-forth"
                                                    : "",
                                                node["name"].asString()));
        }
      } catch (const std::exception &e) {
        spdlog::error("Workspaces: {}", e.what());
      }
    });
  }
  return button;
}

std::string Workspaces::getIcon(const std::string &name, const Json::Value &node) {
  std::vector<std::string> keys = {"high-priority-named", "urgent", "focused", name, "default"};
  for (auto const &key : keys) {
    if (key == "high-priority-named") {
      auto it = std::find_if(high_priority_named_.begin(), high_priority_named_.end(),
                             [&](const std::string &member) { return member == name; });
      if (it != high_priority_named_.end()) {
        return config_["format-icons"][name].asString();
      }

      it = std::find_if(high_priority_named_.begin(), high_priority_named_.end(),
                        [&](const std::string &member) {
                          return trimWorkspaceName(member) == trimWorkspaceName(name);
                        });
      if (it != high_priority_named_.end()) {
        return config_["format-icons"][trimWorkspaceName(name)].asString();
      }
    }
    if (key == "focused" || key == "urgent") {
      if (config_["format-icons"][key].isString() && hasFlag(node, key)) {
        return config_["format-icons"][key].asString();
      }
    } else if (config_["format-icons"]["persistent"].isString() &&
               node["target_output"].isString()) {
      return config_["format-icons"]["persistent"].asString();
    } else if (config_["format-icons"][key].isString()) {
      return config_["format-icons"][key].asString();
    } else if (config_["format-icons"][trimWorkspaceName(key)].isString()) {
      return config_["format-icons"][trimWorkspaceName(key)].asString();
    }
  }
  return name;
}

bool Workspaces::handleScroll(GdkEventScroll *e) {
  if (gdk_event_get_pointer_emulated((GdkEvent *)e) != 0) {
    /**
     * Ignore emulated scroll events on window
     */
    return false;
  }
  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }
  std::string name;
  {
    bool alloutputs = config_["all-outputs"].asBool();
    std::lock_guard<std::mutex> lock(mutex_);
    auto it =
        std::find_if(workspaces_.begin(), workspaces_.end(), [alloutputs](const auto &workspace) {
          if (alloutputs) {
            return hasFlag(workspace, "focused");
          }
          bool noNodes = workspace["nodes"].empty() && workspace["floating_nodes"].empty();
          return hasFlag(workspace, "visible") || (workspace["output"].isString() && noNodes);
        });
    if (it == workspaces_.end()) {
      return true;
    }
    if (dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::RIGHT) {
      name = getCycleWorkspace(it, false);
    } else if (dir == SCROLL_DIR::UP || dir == SCROLL_DIR::LEFT) {
      name = getCycleWorkspace(it, true);
    } else {
      return true;
    }
    if (name == (*it)["name"].asString()) {
      return true;
    }
  }
  if (!config_["warp-on-scroll"].isNull() && !config_["warp-on-scroll"].asBool()) {
    ipc_.sendCmd(IPC_COMMAND, fmt::format("mouse_warping none"));
  }
  try {
    ipc_.sendCmd(IPC_COMMAND, fmt::format(workspace_switch_cmd_, "--no-auto-back-and-forth", name));
  } catch (const std::exception &e) {
    spdlog::error("Workspaces: {}", e.what());
  }
  if (!config_["warp-on-scroll"].isNull() && !config_["warp-on-scroll"].asBool()) {
    ipc_.sendCmd(IPC_COMMAND, fmt::format("mouse_warping container"));
  }
  return true;
}

std::string Workspaces::getCycleWorkspace(std::vector<Json::Value>::iterator it, bool prev) const {
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
    // If a workspace has a focused container then get_tree will say
    // that the workspace itself isn't focused.  Therefore we need to
    // check if any of its nodes are focused as well.
    bool focused = node["focused"].asBool() ||
                   std::any_of(node["nodes"].begin(), node["nodes"].end(),
                               [](const auto &child) { return child["focused"].asBool(); });

    if (focused) {
      button.show();
    } else {
      button.hide();
    }
  } else {
    button.show();
  }
}

}  // namespace waybar::modules::sway
