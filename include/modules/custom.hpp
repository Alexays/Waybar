#pragma once

#include <fmt/format.h>

#include <mutex>
#include <string>
#include <variant>

#include "ALabel.hpp"
#include "util/command.hpp"
#include "util/json.hpp"
#include "util/worker_thread.hpp"

namespace waybar::modules {

class Custom : public ALabel {
 public:
  Custom(const std::string&, const std::string&, const Json::Value&);
  void injectOutput(Json::Value);
  auto update() -> void;
  void refresh(int /*signal*/);

 private:
  void workerExitCallback(int);
  void workerOutputCallback(std::string);
  void parseOutputRaw(const std::string&);
  void parseOutputJson(const std::string&);
  void handleOutputJson(const Json::Value&);
  void handleEvent();
  bool handleScroll(GdkEventScroll* e);
  bool handleToggle(GdkEventButton* const& e);

  const std::string        name_;
  std::string              text_;
  std::string              alt_;
  std::string              tooltip_;
  std::vector<std::string> class_;
  int                      percentage_;
  // Worker exit code, worker raw output, or injected JSON.
  // Injected JSON is string for raw output, object for JSON output.
  std::variant<std::monostate, int, std::string, Json::Value> output_;
  util::JsonParser                                            parser_;
  // Protects output_ since it is accessed from many threads.
  std::mutex                 output_mutex_;
  waybar::util::WorkerThread thread_;
};

}  // namespace waybar::modules
