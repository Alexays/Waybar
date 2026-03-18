#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <system_error>

#include "modules/hyprland/backend.hpp"

namespace fs = std::filesystem;
namespace hyprland = waybar::modules::hyprland;

namespace {
class IPCTestHelper : public hyprland::IPC {
 public:
  static void resetSocketFolder() { socketFolder_.clear(); }
};

std::size_t countOpenFds() {
#if defined(__linux__)
  std::size_t count = 0;
  for (const auto& _ : fs::directory_iterator("/proc/self/fd")) {
    (void)_;
    ++count;
  }
  return count;
#else
  return 0;
#endif
}
}  // namespace

TEST_CASE("XDGRuntimeDirExists", "[getSocketFolder]") {
  // Test case: XDG_RUNTIME_DIR exists and contains "hypr" directory
  // Arrange
  constexpr auto instanceSig = "instance_sig";
  const fs::path tempDir = fs::temp_directory_path() / "hypr_test/run/user/1000";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::path expectedPath = tempDir / "hypr" / instanceSig;
  fs::create_directories(expectedPath);
  setenv("XDG_RUNTIME_DIR", tempDir.c_str(), 1);
  IPCTestHelper::resetSocketFolder();

  // Act
  fs::path actualPath = hyprland::IPC::getSocketFolder(instanceSig);

  // Assert expected result
  REQUIRE(actualPath == expectedPath);
  fs::remove_all(tempDir, ec);
}

TEST_CASE("XDGRuntimeDirDoesNotExist", "[getSocketFolder]") {
  // Test case: XDG_RUNTIME_DIR does not exist
  // Arrange
  constexpr auto instanceSig = "instance_sig";
  unsetenv("XDG_RUNTIME_DIR");
  fs::path expectedPath = fs::path("/tmp") / "hypr" / instanceSig;
  IPCTestHelper::resetSocketFolder();

  // Act
  fs::path actualPath = hyprland::IPC::getSocketFolder(instanceSig);

  // Assert expected result
  REQUIRE(actualPath == expectedPath);
}

TEST_CASE("XDGRuntimeDirExistsNoHyprDir", "[getSocketFolder]") {
  // Test case: XDG_RUNTIME_DIR exists but does not contain "hypr" directory
  // Arrange
  constexpr auto instanceSig = "instance_sig";
  fs::path tempDir = fs::temp_directory_path() / "hypr_test/run/user/1000";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::create_directories(tempDir);
  setenv("XDG_RUNTIME_DIR", tempDir.c_str(), 1);
  fs::path expectedPath = fs::path("/tmp") / "hypr" / instanceSig;
  IPCTestHelper::resetSocketFolder();

  // Act
  fs::path actualPath = hyprland::IPC::getSocketFolder(instanceSig);

  // Assert expected result
  REQUIRE(actualPath == expectedPath);
  fs::remove_all(tempDir, ec);
}

TEST_CASE("Socket folder is resolved per instance signature", "[getSocketFolder]") {
  const fs::path tempDir = fs::temp_directory_path() / "hypr_test/run/user/1000";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::create_directories(tempDir / "hypr");
  setenv("XDG_RUNTIME_DIR", tempDir.c_str(), 1);
  IPCTestHelper::resetSocketFolder();

  const auto firstPath = hyprland::IPC::getSocketFolder("instance_a");
  const auto secondPath = hyprland::IPC::getSocketFolder("instance_b");

  REQUIRE(firstPath == tempDir / "hypr" / "instance_a");
  REQUIRE(secondPath == tempDir / "hypr" / "instance_b");
  REQUIRE(firstPath != secondPath);

  fs::remove_all(tempDir, ec);
}

TEST_CASE("getSocket1Reply throws on no socket", "[getSocket1Reply]") {
  unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  IPCTestHelper::resetSocketFolder();
  std::string request = "test_request";

  CHECK_THROWS(hyprland::IPC::getSocket1Reply(request));
}

#if defined(__linux__)
TEST_CASE("getSocket1Reply failure paths do not leak fds", "[getSocket1Reply][fd-leak]") {
  const auto baseline = countOpenFds();

  unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  for (int i = 0; i < 16; ++i) {
    IPCTestHelper::resetSocketFolder();
    CHECK_THROWS(hyprland::IPC::getSocket1Reply("test_request"));
  }
  const auto after_missing_signature = countOpenFds();
  REQUIRE(after_missing_signature == baseline);

  setenv("HYPRLAND_INSTANCE_SIGNATURE", "definitely-not-running", 1);
  for (int i = 0; i < 16; ++i) {
    IPCTestHelper::resetSocketFolder();
    CHECK_THROWS(hyprland::IPC::getSocket1Reply("test_request"));
  }
  const auto after_connect_failures = countOpenFds();
  REQUIRE(after_connect_failures == baseline);
}
#endif
