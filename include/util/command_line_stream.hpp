#pragma once

#include <glibmm/main.h>
#include <glibmm/spawn.h>

#include <functional>
#include <string>

namespace waybar::util::command {

class LineStream {
 public:
  using OutputCallback = std::function<void(const std::string&)>;
  using ExitCallback = std::function<void(int)>;

  LineStream(std::string output_name, OutputCallback on_output, ExitCallback on_exit);
  ~LineStream();

  void start(const std::string& cmd);
  void stop();
  bool running() const;

 private:
  bool handleStdout(Glib::IOCondition condition);
  void handleExit(Glib::Pid pid, int status);
  void closeStdout();
  void drainStdout(bool flush_trailing_line);
  static int statusToExitCode(int status);

  std::string output_name_;
  OutputCallback on_output_;
  ExitCallback on_exit_;
  std::string buffer_;
  Glib::Pid pid_;
  int stdout_fd_;
  sigc::connection stdout_connection_;
  sigc::connection child_connection_;
};

}  // namespace waybar::util::command
