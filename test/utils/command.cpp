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
#include <string>
#include <vector>

std::mutex reap_mtx;
std::list<pid_t> reap;

extern "C" int waybar_test_execl(const char* path, const char* arg, ...);
extern "C" int waybar_test_execlp(const char* file, const char* arg, ...);

#define execl waybar_test_execl
#define execlp waybar_test_execlp
#include "util/command.hpp"
#undef execl
#undef execlp

constexpr int kInheritedFdExitCode = 42;
static int watched_fd = -1;

// Exits instead of failing the exec if `watched_fd` would have survived it.
static void checkWatchedFd() {
  if (watched_fd != -1 && (fcntl(watched_fd, F_GETFD) & FD_CLOEXEC) == 0) {
    _exit(kInheritedFdExitCode);
  }
}

extern "C" int waybar_test_execl(const char* path, const char* arg, ...) {
  (void)path;
  (void)arg;
  checkWatchedFd();
  errno = ENOENT;
  return -1;
}

extern "C" int waybar_test_execlp(const char* file, const char* arg, ...) {
  (void)file;
  (void)arg;
  checkWatchedFd();
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

TEST_CASE("command children do not inherit descriptors lacking close-on-exec", "[util][command]") {
  watched_fd = ::open("/dev/null", O_RDONLY);
  REQUIRE(watched_fd > STDERR_FILENO);
  REQUIRE((fcntl(watched_fd, F_GETFD) & FD_CLOEXEC) == 0);

  const auto result = waybar::util::command::execNoRead("echo should-not-run");
  const auto pid = waybar::util::command::forkExec("echo should-not-run");
  int status = -1;
  const auto waited = waitpid(pid, &status, 0);
  const int parent_flags = fcntl(watched_fd, F_GETFD);
  ::close(watched_fd);
  watched_fd = -1;

  REQUIRE(result.exit_code == waybar::util::command::kExecFailureExitCode);
  REQUIRE(waited == pid);
  REQUIRE(WIFEXITED(status));
  REQUIRE(WEXITSTATUS(status) == waybar::util::command::kExecFailureExitCode);
  // Only the children's copies are flagged
  REQUIRE((parent_flags & FD_CLOEXEC) == 0);

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

namespace {

// Rewound temporary file holding the given lines, each terminated by a newline
auto fileWithLines(const std::vector<std::string>& lines) -> FILE* {
  auto* fp = tmpfile();
  REQUIRE(fp != nullptr);
  for (const auto& line : lines) {
    REQUIRE(fwrite(line.data(), 1, line.size(), fp) == line.size());
    REQUIRE(fputc('\n', fp) == '\n');
  }
  rewind(fp);
  return fp;
}

}  // namespace

TEST_CASE("command::read bounds what it keeps of the output", "[util][command]") {
  using waybar::util::command::kMaxOutputSize;

  SECTION("output at the limit is kept whole") {
    const std::string line(kMaxOutputSize, 'A');
    auto* fp = fileWithLines({line});
    REQUIRE(waybar::util::command::read(fp) == line);
    fclose(fp);
  }

  SECTION("output past the limit is drained but not kept") {
    auto* fp = fileWithLines({std::string(4 * kMaxOutputSize, 'A')});
    REQUIRE(waybar::util::command::read(fp) == std::string(kMaxOutputSize, 'A'));
    REQUIRE(feof(fp) != 0);
    fclose(fp);
  }
}

TEST_CASE("command::readLine bounds what it keeps of a line", "[util][command]") {
  using waybar::util::command::kMaxOutputSize;

  auto* fp = fileWithLines({"first", "", std::string(4 * kMaxOutputSize, 'A'), "last"});
  std::string line;
  REQUIRE(waybar::util::command::readLine(fp, line));
  REQUIRE(line == "first");
  REQUIRE(waybar::util::command::readLine(fp, line));
  REQUIRE(line.empty());
  REQUIRE(waybar::util::command::readLine(fp, line));
  REQUIRE(line == std::string(kMaxOutputSize, 'A'));
  REQUIRE(waybar::util::command::readLine(fp, line));
  REQUIRE(line == "last");
  REQUIRE_FALSE(waybar::util::command::readLine(fp, line));
  fclose(fp);
}
