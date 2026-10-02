#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
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
  const auto pid =
      waybar::util::command::forkExec("echo should-not-run", "test-output", reap_mtx, reap);
  REQUIRE(pid > 0);

  int status = -1;
  REQUIRE(waitpid(pid, &status, 0) == pid);
  REQUIRE(WIFEXITED(status));
  REQUIRE(WEXITSTATUS(status) == waybar::util::command::kExecFailureExitCode);

  std::scoped_lock<std::mutex> lock(reap_mtx);
  reap.remove(pid);
}

TEST_CASE("command::read returns on a stream whose reads fail", "[util][command]") {
  // A regression here does not fail, it hangs: read() looping on feof() alone
  // never notices that fgets() returned nullptr for an error rather than for
  // end of file.  Run it in a child so the watchdog reports a failure instead
  // of wedging the test run.
  const auto pid = fork();
  REQUIRE(pid >= 0);

  if (pid == 0) {
    // read(2) on a directory fd fails with EISDIR.
    auto* fp = fdopen(::open("/", O_RDONLY), "r");
    if (fp == nullptr) {
      _exit(2);
    }
    waybar::util::command::read(fp);
    _exit(0);
  }

  int status = -1;
  pid_t waited = 0;
  for (int i = 0; i < 50; ++i) {
    waited = waitpid(pid, &status, WNOHANG);
    if (waited != 0) {
      break;
    }
    usleep(100000);
  }

  if (waited == 0) {
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    FAIL("command::read did not return within 5s");
  }

  REQUIRE(waited == pid);
  REQUIRE(WIFEXITED(status));
  REQUIRE(WEXITSTATUS(status) == 0);
}
