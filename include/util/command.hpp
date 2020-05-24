#pragma once

#include <giomm.h>
#include <spdlog/spdlog.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>

namespace waybar::util::command {

struct res {
  int         exit_code;
  std::string out;
};

inline std::string read(FILE* fp) {
  std::array<char, 128> buffer = {0};
  std::string           output;
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

  fclose(fp);
  do {
    waitpid(pid, &stat, WCONTINUED | WUNTRACED);

    if (WIFEXITED(stat)) {
      spdlog::debug("Cmd exited with code {}", WEXITSTATUS(stat));
    } else if (WIFSIGNALED(stat)) {
      spdlog::debug("Cmd killed by {}", WTERMSIG(stat));
    } else if (WIFSTOPPED(stat)) {
      spdlog::debug("Cmd stopped by {}", WSTOPSIG(stat));
    } else if (WIFCONTINUED(stat)) {
      spdlog::debug("Cmd continued");
    } else {
      break;
    }
  } while (!WIFEXITED(stat) && !WIFSIGNALED(stat));
  return stat;
}

inline FILE* open(const std::string& cmd, int& pid) {
  if (cmd == "") return nullptr;
  int fd[2];
  pipe(fd);

  pid_t child_pid = fork();

  if (child_pid < 0) {
    spdlog::error("Unable to exec cmd {}, error {}", cmd.c_str(), strerror(errno));
    return nullptr;
  }

  if (!child_pid) {
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
  int  pid;
  auto fp = command::open(cmd, pid);
  if (!fp) return {-1, ""};
  auto output = command::read(fp);
  auto stat = command::close(fp, pid);
  return {WEXITSTATUS(stat), output};
}

inline struct res execNoRead(const std::string& cmd) {
  int  pid;
  auto fp = command::open(cmd, pid);
  if (!fp) return {-1, ""};
  auto stat = command::close(fp, pid);
  return {WEXITSTATUS(stat), ""};
}

inline int32_t forkExec(const std::string& cmd) {
  if (cmd == "") return -1;

  int32_t pid = fork();

  if (pid < 0) {
    spdlog::error("Unable to exec cmd {}, error {}", cmd.c_str(), strerror(errno));
    return pid;
  }

  // Child executes the command
  if (!pid) {
    setpgid(pid, pid);
    signal(SIGCHLD, SIG_DFL);
    execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)0);
    exit(0);
  } else {
    signal(SIGCHLD, SIG_IGN);
  }

  return pid;
}

}  // namespace waybar::util::command
