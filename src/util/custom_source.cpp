#include "util/custom_source.hpp"

#include <glibmm/markup.h>
#include <glibmm/ustring.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <csignal>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "util/interval.hpp"
#include "util/json.hpp"

namespace waybar::util {

CustomSource::CustomSource(const Json::Value& config, std::string output_name,
                           std::function<void()> on_update)
    : config_(config),
      output_name_(std::move(output_name)),
      on_update_(std::move(on_update)),
      interval_(parseInterval(config_, 0)) {
  // Worker-mode selection copied verbatim from modules::Custom.
  if (!config_["signal"].empty() && config_["interval"].empty() &&
      config_["restart-interval"].empty()) {
    waitingWorker();
  } else if (interval_.count() > 0) {
    delayWorker();
  } else if (config_["exec"].isString()) {
    continuousWorker();
  }
}

CustomSource::~CustomSource() {
  restart_connection_.disconnect();
  if (continuous_stream_) {
    continuous_stream_->stop();
  }
}

void CustomSource::delayWorker() {
  if (!config_["exec"].isString() && !config_["exec-if"].isString()) {
    on_update_();
    return;
  }
  thread_ = [this] {
    bool can_update = true;
    if (config_["exec-if"].isString()) {
      output_ = command::execNoRead(config_["exec-if"].asString());
      if (output_.exit_code != 0) {
        can_update = false;
        on_update_();
      }
    }
    if (can_update) {
      if (config_["exec"].isString()) {
        output_ = command::exec(config_["exec"].asString(), output_name_);
      }
      on_update_();
    }
    thread_.sleep_for(interval_);
  };
}

void CustomSource::continuousWorker() {
  continuous_stream_ = std::make_unique<command::LineStream>(
      output_name_,
      [this](const std::string& output) {
        output_ = {.exit_code = 0, .out = output};
        on_update_();
      },
      [this](int exit_code) { handleContinuousProcessExit(exit_code); });
  startContinuousProcess(true);
}

void CustomSource::startContinuousProcess(bool throw_on_failure) {
  const auto cmd = config_["exec"].asString();
  try {
    continuous_stream_->start(cmd);
  } catch (const Glib::SpawnError& e) {
    if (throw_on_failure) {
      throw std::runtime_error("Unable to open " + cmd + ": " + e.what().raw());
    }
    output_ = {.exit_code = 1, .out = ""};
    on_update_();
    spdlog::error("Unable to restart custom source: {}", e.what().raw());
    scheduleContinuousRestart();
  } catch (const std::exception& e) {
    if (throw_on_failure) {
      throw;
    }
    output_ = {.exit_code = 1, .out = ""};
    on_update_();
    spdlog::error("Unable to restart custom source: {}", e.what());
    scheduleContinuousRestart();
  }
}

void CustomSource::handleContinuousProcessExit(int exit_code) {
  if (exit_code != 0) {
    output_ = {.exit_code = exit_code, .out = ""};
    on_update_();
    spdlog::error("Custom source stopped unexpectedly, is it endless?");
  }
  scheduleContinuousRestart();
}

void CustomSource::scheduleContinuousRestart() {
  restart_connection_.disconnect();
  if (!config_["restart-interval"].isNumeric() || config_["restart-interval"].asDouble() <= 0) {
    // A non-positive restart-interval must not busy-respawn the script.
    return;
  }
  restart_connection_ = Glib::signal_timeout().connect(
      [this] {
        startContinuousProcess(false);
        return false;
      },
      std::max(1U, static_cast<unsigned>(config_["restart-interval"].asDouble() * 1000)));
}

void CustomSource::waitingWorker() {
  thread_ = [this] {
    bool can_update = true;
    if (config_["exec-if"].isString()) {
      output_ = command::execNoRead(config_["exec-if"].asString());
      if (output_.exit_code != 0) {
        can_update = false;
        on_update_();
      }
    }
    if (can_update) {
      if (config_["exec"].isString()) {
        output_ = command::exec(config_["exec"].asString(), output_name_);
      }
      on_update_();
    }
    thread_.sleep();
  };
}

void CustomSource::refresh(int sig) {
#ifdef SIGRTMIN
  if (config_["signal"].isInt() && sig == SIGRTMIN + config_["signal"].asInt()) {
    thread_.wake_up();
  }
#endif
}

CustomSource::Output CustomSource::parse() const { return parse(output_); }

CustomSource::Output CustomSource::parse(const command::res& output) const {
  if (config_["return-type"].asString() == "json") {
    return parseJson(output);
  }
  return parseRaw(output);
}

CustomSource::Output CustomSource::parseRaw(const command::res& output) const {
  Output out;
  // A failed or empty read is stale, not a value: the consumer keeps its last one.
  if (output.out.empty() || output.exit_code != 0) {
    return out;
  }
  const bool escape = config_["escape"].isBool() && config_["escape"].asBool();
  auto clean = [escape](const std::string& s) -> std::string {
    Glib::ustring value = s;
    if (!value.validate()) {
      value = value.make_valid();
    }
    return escape ? Glib::Markup::escape_text(value).raw() : value.raw();
  };
  std::istringstream stream(output.out);
  std::string line;
  int i = 0;
  while (getline(stream, line)) {
    if (i == 0) {
      out.text = clean(line);
      out.tooltip = out.text;
    } else if (i == 1) {
      out.tooltip = clean(line);
    } else if (i == 2) {
      out.classes.push_back(clean(line));
    } else {
      break;
    }
    i++;
  }
  out.ok = !out.text.empty();
  return out;
}

CustomSource::Output CustomSource::parseJson(const command::res& output) const {
  Output out;
  if (output.out.empty() || output.exit_code != 0) {
    return out;
  }
  const bool escape = config_["escape"].isBool() && config_["escape"].asBool();
  auto clean = [escape](const std::string& s) -> std::string {
    Glib::ustring value = s;
    if (!value.validate()) {
      value = value.make_valid();
    }
    return escape ? Glib::Markup::escape_text(value).raw() : value.raw();
  };
  std::istringstream stream(output.out);
  std::string line;
  if (!getline(stream, line)) {
    return out;
  }
  JsonParser parser;
  auto parsed = parser.parse(line);
  out.text = clean(parsed["text"].asString());
  out.tooltip = clean(parsed["tooltip"].asString());
  if (parsed["class"].isString()) {
    out.classes.push_back(parsed["class"].asString());
  } else if (parsed["class"].isArray()) {
    for (auto const& c : parsed["class"]) {
      out.classes.push_back(c.asString());
    }
  }
  // Leave percentage nullopt when absent/non-numeric -- never a synthesised 0 --
  // and do not clamp; the consumer clamps to its own [min, max].
  if (!parsed["percentage"].asString().empty() && parsed["percentage"].isNumeric()) {
    out.percentage = static_cast<int>(std::lround(parsed["percentage"].asFloat()));
  }
  out.ok = true;
  return out;
}

}  // namespace waybar::util
