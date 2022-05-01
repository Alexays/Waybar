#pragma once

#include <giomm.h>
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif
#ifdef __FreeBSD__
#include <sys/procctl.h>
#endif

#include <array>

extern std::mutex reap_mtx;
extern std::list<pid_t> reap;

namespace waybar::util::command {

struct res {
  int exit_code;
  std::string out;
};

inline std::string read(FILE* fp) {
  std::array<char, 128> buffer = {0};
  std::string output;
  while (feof(fp) == 0) {
    if (fgets(buffer.data(), 128, fp) != nullptr) {
      output += buffer.data();
    }
  }

  // Remove last newline
  if (!output.empty() && output[output.length() - 1] == '\n') {
    output.erase(output.length() - 1);
  }
  return output;
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
    } else {
      break;
    }
  } while (!WIFEXITED(stat) && !WIFSIGNALED(stat));
  return stat;
}

inline FILE* open(const std::string& cmd, int& pid) {
  if (cmd == "") return nullptr;
  int fd[2];
  if (pipe(fd) != 0) {
    spdlog::error("Unable to pipe fd");
    return nullptr;
  }

  pid_t child_pid = fork();

  if (child_pid < 0) {
    spdlog::error("Unable to exec cmd {}, error {}", cmd.c_str(), strerror(errno));
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
    setpgid(child_pid, child_pid);
    execlp("/bin/sh", "sh", "-c", cmd.c_str(), (char*)0);
    exit(0);
  } else {
    ::close(fd[1]);
  }
  pid = child_pid;
  return fdopen(fd[0], "r");
}

inline struct res exec(const std::string& cmd) {
  int pid;
  auto fp = command::open(cmd, pid);
  if (!fp) return {-1, ""};
  auto output = command::read(fp);
  auto stat = command::close(fp, pid);
  return {WEXITSTATUS(stat), output};
}

inline struct res execNoRead(const std::string& cmd) {
  int pid;
  auto fp = command::open(cmd, pid);
  if (!fp) return {-1, ""};
  auto stat = command::close(fp, pid);
  return {WEXITSTATUS(stat), ""};
}

inline int32_t forkExec(const std::string& cmd) {
  if (cmd == "") return -1;

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
    setpgid(pid, pid);
    execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)0);
    exit(0);
  } else {
    reap_mtx.lock();
    reap.push_back(pid);
    reap_mtx.unlock();
    spdlog::debug("Added child to reap list: {}", pid);
  }

  return pid;
}

}  // namespace waybar::util::command
