#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <list>
#include <mutex>

std::mutex reap_mtx;
std::list<pid_t> reap;

extern "C" int waybar_test_execl(const char* path, const char* arg, ...);
extern "C" int waybar_test_execlp(const char* file, const char* arg, ...);

#define execl waybar_test_execl
#define execlp waybar_test_execlp
#include "util/command.hpp"
#undef execl
#undef execlp

extern "C" int waybar_test_execl(const char* path, const char* arg, ...) {
  (void)path;
  (void)arg;
  errno = ENOENT;
  return -1;
}

extern "C" int waybar_test_execlp(const char* file, const char* arg, ...) {
  (void)file;
  (void)arg;
  errno = ENOENT;
  return -1;
}

TEST_CASE("command::execNoRead returns 127 when shell exec fails", "[util][command]") {
  const auto result = waybar::util::command::execNoRead("echo should-not-run");
  REQUIRE(result.exit_code == waybar::util::command::kExecFailureExitCode);
  REQUIRE(result.out.empty());
}

TEST_CASE("command::forkExec child exits 127 when shell exec fails", "[util][command]") {
  const auto pid = waybar::util::command::forkExec("echo should-not-run", "test-output");
  REQUIRE(pid > 0);

  int status = -1;
  REQUIRE(waitpid(pid, &status, 0) == pid);
  REQUIRE(WIFEXITED(status));
  REQUIRE(WEXITSTATUS(status) == waybar::util::command::kExecFailureExitCode);

  std::scoped_lock<std::mutex> lock(reap_mtx);
  reap.remove(pid);
}
