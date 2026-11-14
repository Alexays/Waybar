#pragma once

#include <fcntl.h>
#include <giomm.h>
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#if __has_include(<linux/close_range.h>)
#include <linux/close_range.h>
#endif
#endif
#ifdef __FreeBSD__
#include <sys/procctl.h>
#endif

#include <array>
#include <string_view>

#include "util/scope_guard.hpp"
#include "util/sleeper_thread.hpp"

extern std::mutex reap_mtx;
extern std::list<pid_t> reap;

namespace waybar::util::command {

constexpr int kExecFailureExitCode = 127;
// How much of a command's output (of each line, for a continuous command) is kept
constexpr size_t kMaxOutputSize = 256 * 1024;

struct res {
  int exit_code;
  std::string out;
};

// Runs in a forked child before exec, so that commands do not inherit the bar's descriptors.
inline void setInheritedFdsCloexec() {
#if defined(__linux__) && defined(SYS_close_range) && defined(CLOSE_RANGE_CLOEXEC)
  if (syscall(SYS_close_range, STDERR_FILENO + 1, ~0U, CLOSE_RANGE_CLOEXEC) == 0) return;
#elif defined(__FreeBSD__) && defined(CLOSE_RANGE_CLOEXEC)
  if (close_range(STDERR_FILENO + 1, ~0U, CLOSE_RANGE_CLOEXEC) == 0) return;
#endif
  const long max_fd = sysconf(_SC_OPEN_MAX);
  for (int fd = STDERR_FILENO + 1; fd < max_fd; ++fd) {
    fcntl(fd, F_SETFD, FD_CLOEXEC);
  }
}

inline void truncateOutput(std::string& output) {
  if (output.size() > kMaxOutputSize) {
    spdlog::warn("Command output truncated to {} bytes", kMaxOutputSize);
    output.resize(kMaxOutputSize);
  }
}

inline std::string read(FILE* fp) {
  std::array<char, 128> buffer = {0};
  std::string output;
  while (fgets(buffer.data(), buffer.size(), fp) != nullptr) {
    // Past the limit, keep draining the pipe so that the command can still finish
    if (output.size() <= kMaxOutputSize) {
      output += buffer.data();
    }
  }
  if (ferror(fp) != 0) {
    spdlog::error("Error reading command output: {}", strerror(errno));
  }

  // Remove last newline
  if (!output.empty() && output[output.length() - 1] == '\n') {
    output.erase(output.length() - 1);
  }
  truncateOutput(output);
  return output;
}

// Like getline(3) without the newline, bounded. Returns false when there is nothing left to read.
inline bool readLine(FILE* fp, std::string& line) {
  std::array<char, 128> buffer = {0};
  bool found = false;
  line.clear();
  while (fgets(buffer.data(), buffer.size(), fp) != nullptr) {
    found = true;
    std::string_view chunk = buffer.data();
    const bool complete = chunk.ends_with('\n');
    if (complete) {
      chunk.remove_suffix(1);
    }
    if (line.size() <= kMaxOutputSize) {
      line += chunk;
    }
    if (complete) {
      break;
    }
  }
  truncateOutput(line);
  return found;
}

inline int close(FILE* fp, pid_t pid) {
  int stat = -1;
  pid_t ret;

  fclose(fp);
  do {
    ret = waitpid(pid, &stat, WCONTINUED | WUNTRACED);

    if (WIFEXITED(stat)) {
      spdlog::debug("Cmd exited with code {}", WEXITSTATUS(stat));
    } else if (WIFSIGNALED(stat)) {
      spdlog::debug("Cmd killed by {}", WTERMSIG(stat));
    } else if (WIFSTOPPED(stat)) {
      spdlog::debug("Cmd stopped by {}", WSTOPSIG(stat));
    } else if (WIFCONTINUED(stat)) {
      spdlog::debug("Cmd continued");
    } else if (ret == -1) {
      spdlog::debug("waitpid failed: {}", strerror(errno));
      break;
    } else {
      break;
    }
  } while (!WIFEXITED(stat) && !WIFSIGNALED(stat));
  return stat;
}

inline FILE* open(const std::string& cmd, int& pid, const std::string& output_name) {
  if (cmd == "") return nullptr;
  // Module workers are stopped with pthread_cancel() (SleeperThread::stop()). A cancel request
  // that is pending when the worker forks is inherited by the child, which then acts on it at its
  // first cancellation point (close() below) instead of reaching execlp(): it unwinds the copy of
  // the worker's stack, and the exit of its only thread runs exit(0) with Waybar's static
  // destructors. The parent is cancelled before close() calls waitpid(), so that child is left a
  // zombie. Keep cancellation disabled until the pid and the pipe are handed back; the child
  // inherits the disabled state.
  CancellationGuard cancel_lock;
  int fd[2];
  // Open the pipe with the close-on-exec flag set, so it will not be inherited
  // by any other subprocesses launched by other threads (which could result in
  // the pipe staying open after this child dies, causing us to hang when trying
  // to read from it)
  if (pipe2(fd, O_CLOEXEC) != 0) {
    spdlog::error("Unable to pipe fd");
    return nullptr;
  }

  pid_t child_pid = fork();

  if (child_pid < 0) {
    spdlog::error("Unable to exec cmd {}, error {}", cmd.c_str(), strerror(errno));
    ::close(fd[0]);
    ::close(fd[1]);
    return nullptr;
  }

  if (!child_pid) {
    int err;
    sigset_t mask;
    sigfillset(&mask);
    // Reset sigmask
    err = pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);
    if (err != 0) spdlog::error("pthread_sigmask in open failed: {}", strerror(err));
    // Kill child if Waybar exits
    int deathsig = SIGTERM;
#ifdef __linux__
    if (prctl(PR_SET_PDEATHSIG, deathsig) != 0) {
      spdlog::error("prctl(PR_SET_PDEATHSIG) in open failed: {}", strerror(errno));
    }
#endif
#ifdef __FreeBSD__
    if (procctl(P_PID, 0, PROC_PDEATHSIG_CTL, reinterpret_cast<void*>(&deathsig)) == -1) {
      spdlog::error("procctl(PROC_PDEATHSIG_CTL) in open failed: {}", strerror(errno));
    }
#endif
    ::close(fd[0]);
    dup2(fd[1], 1);
    setInheritedFdsCloexec();
    setpgid(child_pid, child_pid);
    if (!output_name.empty()) {
      setenv("WAYBAR_OUTPUT_NAME", output_name.c_str(), 1);
    }
    execlp("/bin/sh", "sh", "-c", cmd.c_str(), (char*)0);
    const int saved_errno = errno;
    spdlog::error("execlp(/bin/sh) failed in open: {}", strerror(saved_errno));
    _exit(kExecFailureExitCode);
  } else {
    ::close(fd[1]);
  }
  pid = child_pid;
  return fdopen(fd[0], "r");
}

// Cleanup for exec()/execNoRead() when they are left early, which in practice means the worker was
// cancelled while it read the output or waited for the child: nobody would call waitpid() for that
// child, so close the pipe and hand a still running child over to signalThread() via reap.
inline void abandon(FILE* fp, pid_t pid) {
  CancellationGuard cancel_lock;
  if (fp != nullptr) fclose(fp);
  if (pid > 0 && waitpid(pid, nullptr, WNOHANG) == 0) {
    std::lock_guard<std::mutex> lock(reap_mtx);
    reap.push_back(pid);
  }
}

inline struct res exec(const std::string& cmd, const std::string& output_name) {
  int pid;
  auto fp = command::open(cmd, pid, output_name);
  if (!fp) return {-1, ""};
  FILE* pending_fp = fp;
  pid_t pending_pid = pid;
  ScopeGuard on_early_exit([&pending_fp, &pending_pid]() { abandon(pending_fp, pending_pid); });
  auto output = command::read(fp);
  pending_fp = nullptr;  // close() owns fp from here on
  auto stat = command::close(fp, pid);
  pending_pid = -1;
  return {WEXITSTATUS(stat), output};
}

inline struct res execNoRead(const std::string& cmd) {
  int pid;
  auto fp = command::open(cmd, pid, "");
  if (!fp) return {-1, ""};
  pid_t pending_pid = pid;
  ScopeGuard on_early_exit([&pending_pid]() { abandon(nullptr, pending_pid); });
  auto stat = command::close(fp, pid);
  pending_pid = -1;
  return {WEXITSTATUS(stat), ""};
}

inline int32_t forkExec(const std::string& cmd, const std::string& output_name) {
  if (cmd == "") return -1;
  // See open(): the child inherits the disabled state and reaches execl().
  CancellationGuard cancel_lock;

  pid_t pid = fork();

  if (pid < 0) {
    spdlog::error("Unable to exec cmd {}, error {}", cmd.c_str(), strerror(errno));
    return pid;
  }

  // Child executes the command
  if (!pid) {
    int err;
    sigset_t mask;
    sigfillset(&mask);
    // Reset sigmask
    err = pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);
    if (err != 0) spdlog::error("pthread_sigmask in forkExec failed: {}", strerror(err));
    setInheritedFdsCloexec();
    setpgid(pid, pid);
    if (!output_name.empty()) {
      setenv("WAYBAR_OUTPUT_NAME", output_name.c_str(), 1);
    }
    execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)0);
    const int saved_errno = errno;
    spdlog::error("execl(/bin/sh) failed in forkExec: {}", strerror(saved_errno));
    _exit(kExecFailureExitCode);
  } else {
    reap_mtx.lock();
    reap.push_back(pid);
    reap_mtx.unlock();
    spdlog::debug("Added child to reap list: {}", pid);
  }

  return pid;
}

inline int32_t forkExec(const std::string& cmd) { return forkExec(cmd, ""); }

}  // namespace waybar::util::command
