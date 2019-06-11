#pragma once

#include <giomm.h>
#include <sys/wait.h>
#include <unistd.h>

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
  int stat;

  fclose(fp);
  while (waitpid(pid, &stat, 0) == -1) {
    if (errno != EINTR) {
      stat = 0;
      break;
    }
  }

  return stat;
}

inline FILE* open(const std::string cmd, int& pid) {
  if (cmd == "") return nullptr;
  int fd[2];
  pipe(fd);

  pid_t child_pid = fork();

  if (child_pid < 0) {
    printf("Unable to exec cmd %s, error %s", cmd.c_str(), strerror(errno));
    return nullptr;
  }

  if (!child_pid) {
    ::close(fd[0]);
    dup2(fd[1], 1);
    setpgid(child_pid, child_pid);
    execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)0);
    exit(0);
  } else {
    ::close(fd[1]);
  }
  pid = child_pid;
  return fdopen(fd[0], "r");
}

inline struct res exec(std::string cmd) {
  int  pid;
  auto fp = command::open(cmd, pid);
  if (!fp) return {-1, ""};
  auto output = command::read(fp);
  auto stat = command::close(fp, pid);
  return {WEXITSTATUS(stat), output};
}

inline int32_t forkExec(std::string cmd) {
  if (cmd == "") return -1;

  int32_t pid = fork();

  if (pid < 0) {
    printf("Unable to exec cmd %s, error %s", cmd.c_str(), strerror(errno));
    return pid;
  }

  // Child executes the command
  if (!pid) {
    setpgid(pid, pid);
    execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)0);
    exit(0);
  } else {
    signal(SIGCHLD,SIG_IGN);
  }

  return pid;
}

}  // namespace waybar::util::command
