#include "util/command_line_stream.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>

#ifdef __linux__
#include <sys/prctl.h>
#endif
#ifdef __FreeBSD__
#include <sys/procctl.h>
#endif

#include "util/command.hpp"

namespace {

auto buildChildEnvironment(const std::string& output_name) -> std::vector<std::string> {
  auto names = Glib::listenv();
  std::vector<std::string> envp;
  envp.reserve(names.size() + 1);

  for (const auto& name : names) {
    bool found = false;
    auto value = Glib::getenv(name, found);
    if (!found || name == "WAYBAR_OUTPUT_NAME") {
      continue;
    }
    envp.push_back(name + "=" + value);
  }

  if (!output_name.empty()) {
    envp.push_back("WAYBAR_OUTPUT_NAME=" + output_name);
  }

  return envp;
}

void prepareChild() {
  sigset_t mask;
  sigfillset(&mask);

  (void)pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);

  int deathsig = SIGTERM;
#ifdef __linux__
  (void)prctl(PR_SET_PDEATHSIG, deathsig);
#endif
#ifdef __FreeBSD__
  (void)procctl(P_PID, 0, PROC_PDEATHSIG_CTL, reinterpret_cast<void*>(&deathsig));
#endif

  (void)setpgid(0, 0);
}

void emitBufferedLines(std::string& buffer,
                       const waybar::util::command::LineStream::OutputCallback& on_output,
                       bool flush_trailing_line) {
  for (auto newline_pos = buffer.find('\n'); newline_pos != std::string::npos;
       newline_pos = buffer.find('\n')) {
    auto line = buffer.substr(0, newline_pos);
    buffer.erase(0, newline_pos + 1);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    on_output(line);
  }

  if (flush_trailing_line && !buffer.empty()) {
    auto line = std::move(buffer);
    buffer.clear();
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    on_output(line);
  }
}

}  // namespace

waybar::util::command::LineStream::LineStream(std::string output_name, OutputCallback on_output,
                                              ExitCallback on_exit)
    : output_name_(std::move(output_name)),
      on_output_(std::move(on_output)),
      on_exit_(std::move(on_exit)),
      pid_(0),
      stdout_fd_(-1) {}

waybar::util::command::LineStream::~LineStream() { stop(); }

void waybar::util::command::LineStream::start(const std::string& cmd) {
  stop();

  std::vector<std::string> argv{"/bin/sh", "-c", cmd};
  auto envp = buildChildEnvironment(output_name_);
  Glib::spawn_async_with_pipes("", argv, envp,
                               Glib::SPAWN_DO_NOT_REAP_CHILD | Glib::SPAWN_CLOEXEC_PIPES,
                               sigc::ptr_fun(&prepareChild), &pid_, nullptr, &stdout_fd_, nullptr);

  const auto flags = fcntl(stdout_fd_, F_GETFL, 0);
  if (flags == -1 || fcntl(stdout_fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
    const auto saved_errno = errno;
    stop();
    throw std::runtime_error("Unable to configure child stdout: " +
                             std::string(std::strerror(saved_errno)));
  }

  stdout_connection_ =
      Glib::signal_io().connect(sigc::mem_fun(*this, &LineStream::handleStdout), stdout_fd_,
                                Glib::IO_IN | Glib::IO_HUP | Glib::IO_ERR | Glib::IO_NVAL);
  child_connection_ =
      Glib::signal_child_watch().connect(sigc::mem_fun(*this, &LineStream::handleExit), pid_);
}

void waybar::util::command::LineStream::stop() {
  stdout_connection_.disconnect();
  child_connection_.disconnect();

  if (pid_ != 0) {
    killpg(pid_, SIGTERM);
    waitpid(pid_, nullptr, 0);
    Glib::spawn_close_pid(pid_);
    pid_ = 0;
  }

  closeStdout();
  buffer_.clear();
}

bool waybar::util::command::LineStream::running() const { return pid_ != 0; }

bool waybar::util::command::LineStream::handleStdout(Glib::IOCondition condition) {
  const auto should_flush =
      static_cast<bool>(condition & (Glib::IO_HUP | Glib::IO_ERR | Glib::IO_NVAL));
  drainStdout(should_flush);

  if (!running() || should_flush) {
    closeStdout();
    return false;
  }

  return true;
}

void waybar::util::command::LineStream::handleExit(Glib::Pid pid, int status) {
  child_connection_.disconnect();

  if (stdout_fd_ != -1) {
    drainStdout(true);
    stdout_connection_.disconnect();
    closeStdout();
  }

  if (pid_ == pid) {
    Glib::spawn_close_pid(pid_);
    pid_ = 0;
  } else {
    Glib::spawn_close_pid(pid);
  }

  on_exit_(statusToExitCode(status));
}

void waybar::util::command::LineStream::closeStdout() {
  if (stdout_fd_ != -1) {
    ::close(stdout_fd_);
    stdout_fd_ = -1;
  }
}

void waybar::util::command::LineStream::drainStdout(bool flush_trailing_line) {
  if (stdout_fd_ == -1) {
    return;
  }

  std::array<char, 4096> chunk = {};
  while (true) {
    const auto bytes_read = ::read(stdout_fd_, chunk.data(), chunk.size());
    if (bytes_read > 0) {
      buffer_.append(chunk.data(), static_cast<size_t>(bytes_read));
      emitBufferedLines(buffer_, on_output_, false);
      continue;
    }

    if (bytes_read == 0) {
      emitBufferedLines(buffer_, on_output_, flush_trailing_line);
      return;
    }

    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }

    spdlog::error("Reading command stdout failed: {}", std::strerror(errno));
    emitBufferedLines(buffer_, on_output_, flush_trailing_line);
    return;
  }
}

int waybar::util::command::LineStream::statusToExitCode(int status) {
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return 1;
}
