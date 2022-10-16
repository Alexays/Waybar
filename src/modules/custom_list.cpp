#include "modules/custom_list.hpp"

#include <spdlog/spdlog.h>

waybar::modules::CustomList::CustomList(const std::string& name,
                                        const Bar &bar,
                                        const std::string& id,
                                        const Json::Value& config)
    : AModule(config, "custom-list-" + name, id, false, !config["disable-scroll"].asBool()),
      name_(name),
      bar_(bar),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      interval_(config["interval"].isUInt() ? config_["interval"].asUInt() : 0),
      fp_(nullptr),
      pid_(-1) {
  box_.set_name("custom-list-" + name);
  event_box_.add(box_);
  if(!id.empty())
      box_.get_style_context()->add_class(id);
  if (interval_.count() > 0) {
    delayWorker();
  } else if (config_["exec"].isString()) {
    continuousWorker();
  }
}

waybar::modules::CustomList::~CustomList() {
  if (pid_ != -1) {
    killpg(pid_, SIGTERM);
    pid_ = -1;
  }
}

void waybar::modules::CustomList::delayWorker() {
  thread_ = [this] {
    bool can_update = true;
    if (config_["exec-if"].isString()) {
      output_ = util::command::execNoRead(config_["exec-if"].asString());
      if (output_.exit_code != 0) {
        can_update = false;
        dp.emit();
      }
    }
    if (can_update) {
      if (config_["exec"].isString()) {
        output_ = util::command::exec(config_["exec"].asString());
      }
      dp.emit();
    }
    thread_.sleep_for(interval_);
  };
}

void waybar::modules::CustomList::continuousWorker() {
  auto cmd = config_["exec"].asString();
  pid_ = -1;
  fp_ = util::command::open(cmd, pid_);
  if (!fp_) {
    throw std::runtime_error("Unable to open " + cmd);
  }
  thread_ = [this, cmd] {
    char* buff = nullptr;
    size_t len = 0;
    if (getline(&buff, &len, fp_) == -1) {
      int exit_code = 1;
      if (fp_) {
        exit_code = WEXITSTATUS(util::command::close(fp_, pid_));
        fp_ = nullptr;
      }
      if (exit_code != 0) {
        output_ = {exit_code, ""};
        dp.emit();
        spdlog::error("{} stopped unexpectedly, is it endless?", name_);
      }
      if (config_["restart-interval"].isUInt()) {
        pid_ = -1;
        thread_.sleep_for(std::chrono::seconds(config_["restart-interval"].asUInt()));
        fp_ = util::command::open(cmd, pid_);
        if (!fp_) {
          throw std::runtime_error("Unable to open " + cmd);
        }
      } else {
        thread_.stop();
        return;
      }
    } else {
      std::string output = buff;

      // Remove last newline
      if (!output.empty() && output[output.length() - 1] == '\n') {
        output.erase(output.length() - 1);
      }
      output_ = {0, output};
      dp.emit();
    }
  };
}

void waybar::modules::CustomList::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
}

void waybar::modules::CustomList::handleEvent() {
  if (!config_["exec-on-event"].isBool() || config_["exec-on-event"].asBool()) {
    thread_.wake_up();
  }
}

bool waybar::modules::CustomList::handleScroll(GdkEventScroll* e) {
  //auto ret = ALabel::handleScroll(e);
  ///* TODO */
  handleEvent();
  return false;
}

bool waybar::modules::CustomList::handleToggle(GdkEventButton* const& e) {
    /* TODO */
  //auto ret = ALabel::handleToggle(e);
  handleEvent();
  return false;
}

auto waybar::modules::CustomList::update() -> void {
  // Hide label if output is empty
  if ((config_["exec"].isString() || config_["exec-if"].isString()) &&
      (output_.out.empty() || output_.exit_code != 0)) {
    event_box_.hide();
  } else {
      parseOutputJson();
  }
  bool needReorder = false;
  for(auto res = results_.begin(); res != results_.end(); res++) {
    std::string name = (*res)["name"].asString();
    auto bit = buttons_.find(name);
    if(bit == buttons_.end()) {
      needReorder = true;
    }
    auto &button = bit == buttons_.end() ? addButton(*res) : bit->second;

    auto prev = std::find_if(prev_.begin(), prev_.end(), [name](auto p) {
      return p["name"] == name;
    });

    // Classes
    std::vector<std::string> prev_classes;
    if(prev != prev_.end()) {
      if ((*prev)["class"].isString()) {
        prev_classes.push_back((*prev)["class"].asString());
      } else if ((*prev)["class"].isArray()) {
        for (auto const& c : (*prev)["class"]) {
          prev_classes.push_back(c.asString());
        }
      }
    }
    std::vector<std::string> new_classes;
    if ((*res)["class"].isString()) {
      new_classes.push_back((*res)["class"].asString());
    } else if ((*res)["class"].isArray()) {
      for (auto const& c : (*res)["class"]) {
        new_classes.push_back(c.asString());
      }
    }

    for(auto it : prev_classes) {
      if(std::find(new_classes.begin(), new_classes.end(), it) == new_classes.end())
        button.get_style_context()->remove_class(it);
    }
    for(auto it : new_classes) {
      if(std::find(prev_classes.begin(), prev_classes.end(), it) == prev_classes.end())
        button.get_style_context()->add_class(it);
    }

    // Reorder to match results order
    if(needReorder) {
      box_.reorder_child(button, res - results_.begin());
    }

    if(!(*res)["disable-markup"].asBool()) {
      static_cast<Gtk::Label *>(button.get_children()[0])->set_markup((*res)["text"].asString());
    } else {
      button.set_label((*res)["text"].asString());
    }

    if((*res)["hide"].asBool()) {
      button.hide();
    } else {
      button.show();
    }

    if(button.get_tooltip_markup() != (*res)["tooltip"].asString()) {
      button.set_tooltip_markup((*res)["tooltip"].asString());
    }

  }
  for(auto prev : prev_) {
    auto res_it = std::find_if(results_.begin(), results_.end(), [prev](auto it) {
      return it["name"] == prev["name"];
    });
    if(res_it == results_.end()) {
      // Node has been removed
      auto bit = buttons_.find(prev["name"].asString());
      if(bit != buttons_.end()) {
        auto &button = bit->second;
        box_.remove(button);
        buttons_.erase(bit);
      }
    }

  }
  prev_ = results_;

  // Call parent update
  AModule::update();
}

void waybar::modules::CustomList::handleClick(std::string name) {
  auto node = std::find_if(results_.begin(), results_.end(), [name](auto it) {
    return it["name"] == name;
  });

  if(node == results_.end())
    return;
  auto cmd = (*node)["onclick"];

  if(cmd.isString()) {
    util::command::execNoRead(cmd.asString());
  }
}

Gtk::Button &waybar::modules::CustomList::addButton(const Json::Value &node) {
    auto pair = buttons_.emplace(node["name"].asString(), node["name"].asString());
    auto &&button = pair.first->second;
    box_.pack_start(button, false, false, 0);
    button.set_name(name_ + node["name"].asString());
    button.set_relief(Gtk::RELIEF_NONE);
    if(!config_["disable-click"].asBool()) {
      button.signal_pressed().connect([this, node] {
        handleClick(node["name"].asString());
      });
    }
    return button;
}

void waybar::modules::CustomList::parseOutputJson() {
  std::istringstream output(output_.out);
  std::string line;
  class_.clear();
  while (getline(output, line)) {
    auto parsed = parser_.parse(line);
    if(!parsed["data"].isArray()) {
      throw std::runtime_error("Output should be a list");
    }
    results_.clear();
    for(auto it : parsed["data"]) {
        results_.push_back(it);
    }
    break;
  }
}
