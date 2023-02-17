#include "modules/custom.hpp"

#include <spdlog/spdlog.h>

waybar::modules::Custom::Custom(const std::string& name,
                                const Bar &bar,
                                const std::string& id,
                                const Json::Value& config)
    : AModule(config, "custom-" + name, id, false, !config["disable-scroll"].asBool()),
      interval_(config["interval"].isUInt() ? config_["interval"].asUInt() : 0),
      box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
      format_(config["format"].isString() ? config["format"].asString() : "{}"),
      fp_(nullptr),
      pid_(-1) {
  box_.set_name("custom-" + name);
  event_box_.add(box_);
  if(!id.empty()) {
      box_.get_style_context()->add_class(id);
  }
  dp.emit();
  if (interval_.count() > 0) {
    delayWorker();
  } else if (config_["exec"].isString()) {
    continuousWorker();
  }
}

waybar::modules::Custom::~Custom() {
  if (pid_ != -1) {
    killpg(pid_, SIGTERM);
    waitpid(pid_, NULL, 0);
    pid_ = -1;
  }
}

void waybar::modules::Custom::delayWorker() {
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

void waybar::modules::Custom::continuousWorker() {
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

void waybar::modules::Custom::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
}

void waybar::modules::Custom::handleEvent() {
  if (!config_["exec-on-event"].isBool() || config_["exec-on-event"].asBool()) {
    thread_.wake_up();
  }
}

bool waybar::modules::Custom::handleScroll(GdkEventScroll* e) {
  auto ret = AModule::handleScroll(e);
  handleEvent();
  return ret;
}

bool waybar::modules::Custom::handleToggle(GdkEventButton* const& e) {
  auto ret = AModule::handleToggle(e);
  handleEvent();
  return ret;
}

auto waybar::modules::Custom::update() -> void {
  // Hide label if output is empty
  if ((config_["exec"].isString() || config_["exec-if"].isString()) &&
      (output_.out.empty() || output_.exit_code != 0)) {
    event_box_.hide();
  } else {
    event_box_.show();
    if (config_["return-type"].asString() == "json") {
      parseOutputJson();
    } else {
      parseOutputRaw();
    }
    bool needReorder = false;
    for(auto res = results_.begin(); res != results_.end(); res++) {
      std::string name = res->name_;
      auto bit = buttons_.find(name);
      if(bit == buttons_.end()) {
        needReorder = true;
      }

      auto &button = bit == buttons_.end() ? addButton(*res) : bit->second;

      auto str = fmt::format(format_, res->text_, fmt::arg("alt", res->alt_),
                             fmt::arg("icon", getIcon(res->percentage_, res->alt_)),
                             fmt::arg("percentage", res->percentage_));
      if(str.empty() || res->hide_) {
        button.hide();
      } else {
        Gtk::Label *label_ = static_cast<Gtk::Label *>(button.get_children()[0]);
        label_->set_markup(str);
        button.show();
        auto prev = std::find_if(prev_.begin(), prev_.end(), [name](auto p) {
          return p.name_ == name;
        });
        if(prev != prev_.end()) {
          for(auto it : prev->class_) {
            if(std::find(res->class_.begin(), res->class_.end(), it) == res->class_.end()) {
              button.get_style_context()->remove_class(it);
            }
          }
        }
        for(auto it : res->class_) {
          if(prev  != prev_.end()) {
            if(std::find(prev->class_.begin(), prev->class_.end(), it) == prev->class_.end()) {
              button.get_style_context()->add_class(it);
            }
          } else {
            button.get_style_context()->add_class(it);
          }
        }

        if(needReorder) {
          box_.reorder_child(button, res - results_.begin());
        }

        if(button.get_tooltip_markup() != res->tooltip_) {
          button.set_tooltip_markup(res->tooltip_);
        }
      }
    }

    for(auto prev : prev_) {
      auto res_it = std::find_if(results_.begin(), results_.end(), [prev](auto it) {
        return it.name_ == prev.name_;
      });
      if(res_it == results_.end()) {
        auto bit = buttons_.find(prev.name_);
        if(bit != buttons_.end()) {
          auto &button = bit->second;
          box_.remove(button);
          buttons_.erase(bit);
        }
      }
    }
  }
  prev_ = results_;
  // Call parent update
  AModule::update();
}

std::string waybar::modules::Custom::getIcon(uint16_t percentage, const std::string& alt, uint16_t max) {
  auto format_icons = config_["format-icons"];
  if (format_icons.isObject()) {
    if (!alt.empty() && (format_icons[alt].isString() || format_icons[alt].isArray())) {
      format_icons = format_icons[alt];
    } else {
      format_icons = format_icons["default"];
    }
  }
  if (format_icons.isArray()) {
    auto size = format_icons.size();
    if (size) {
      auto idx = std::clamp(percentage / ((max == 0 ? 100 : max) / size), 0U, size - 1);
      format_icons = format_icons[idx];
    }
  }
  if (format_icons.isString()) {
    return format_icons.asString();
  }
  return "";
}

void waybar::modules::Custom::parseOutputRaw() {
  Node n = Node();
  // Retain name if there is only one node
  n.name_ = name_;
  std::istringstream output(output_.out);
  std::string line;
  int i = 0;
  while (getline(output, line)) {
    if (i == 0) {
      if (config_["escape"].isBool() && config_["escape"].asBool()) {
        n.text_ = Glib::Markup::escape_text(line);
      } else {
        n.text_ = line;
      }
      n.tooltip_ = line;
      n.class_.clear();
    } else if (i == 1) {
      n.tooltip_ = line;
    } else if (i == 2) {
      n.class_.push_back(line);
    } else {
      break;
    }
    i++;
  }
  results_.clear();
  results_.push_back(n);
}

waybar::modules::Custom::Node waybar::modules::Custom::parseItem(Json::Value &parsed) {
  Node n;
  if (config_["escape"].isBool() && config_["escape"].asBool()) {
    n.text_ = Glib::Markup::escape_text(parsed["text"].asString());
  } else {
    n.text_ = parsed["text"].asString();
  }
  if (config_["escape"].isBool() && config_["escape"].asBool()) {
    n.alt_ = Glib::Markup::escape_text(parsed["alt"].asString());
  } else {
    n.alt_ = parsed["alt"].asString();
  }
  n.tooltip_ = parsed["tooltip"].asString();
  if (parsed["class"].isString()) {
    n.class_.push_back(parsed["class"].asString());
  } else if (parsed["class"].isArray()) {
    for (auto const& c : parsed["class"]) {
      n.class_.push_back(c.asString());
    }
  }
  if(!parsed["name"].asString().empty()) {
    n.name_ = name_ + parsed["name"].asString();
  }
  if (!parsed["percentage"].asString().empty() && parsed["percentage"].isNumeric()) {
    n.percentage_ = (int)lround(parsed["percentage"].asFloat());
  } else {
    n.percentage_ = 0;
  }
  if (!parsed["onclick"].asString().empty() && parsed["onclick"].isString()) {
    n.onclick_ = parsed["onclick"].asString();
  }
  if(parsed["hide"].isBool()) {
    n.hide_ = parsed["hide"].asBool();
  }



  return n;
}

void waybar::modules::Custom::parseOutputJson() {
  std::istringstream output(output_.out);
  std::string line;
  while (getline(output, line)) {
    auto parsed = parser_.parse(line);
    results_.clear();
    if(parsed["data"].isArray()) {
      for(auto it : parsed["data"]) {
        results_.push_back(parseItem(it));
      }
    } else {
      Node n = parseItem(parsed);
      n.name_ = name_ + "-node";
      results_.push_back(n);
    }
  }
}

Gtk::Button &waybar::modules::Custom::addButton(const waybar::modules::Custom::Node &node) {
    auto pair = buttons_.emplace(node.name_, node.name_);
    auto &&button = pair.first->second;
    box_.pack_start(button, false, false, 0);
    button.set_name(name_ + node.name_);
    button.set_relief(Gtk::RELIEF_NONE);
    if(!config_["disable-click"].asBool()) {
      button.signal_pressed().connect([this, node] {
        handleClick(node.name_);
      });
    }
    return button;
}


void waybar::modules::Custom::handleClick(std::string name) {
  auto node = std::find_if(results_.begin(), results_.end(), [name](auto it) {
    return it.name_ == name;
  });

  if(node == results_.end())
    return;
  auto cmd = node->onclick_;

  if(!cmd.empty()) {
    util::command::execNoRead(cmd);
  }
}
