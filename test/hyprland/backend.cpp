#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include "fixtures/IPCTestFixture.hpp"

namespace fs = std::filesystem;
namespace hyprland = waybar::modules::hyprland;

TEST_CASE_METHOD(IPCTestFixture, "XDGRuntimeDirExists", "[getSocketFolder]") {
  // Test case: XDG_RUNTIME_DIR exists and contains "hypr" directory
  // Arrange
  tempDir = fs::temp_directory_path() / "hypr_test/run/user/1000";
  fs::path expectedPath = tempDir / "hypr" / instanceSig;
  fs::create_directories(tempDir / "hypr" / instanceSig);
  setenv("XDG_RUNTIME_DIR", tempDir.c_str(), 1);

  // Act
  fs::path actualPath = getSocketFolder(instanceSig);

  // Assert expected result
  REQUIRE(actualPath == expectedPath);
}

TEST_CASE_METHOD(IPCTestFixture, "XDGRuntimeDirDoesNotExist", "[getSocketFolder]") {
  // Test case: XDG_RUNTIME_DIR does not exist
  // Arrange
  unsetenv("XDG_RUNTIME_DIR");
  fs::path expectedPath = fs::path("/tmp") / "hypr" / instanceSig;

  // Act
  fs::path actualPath = getSocketFolder(instanceSig);

  // Assert expected result
  REQUIRE(actualPath == expectedPath);
}

TEST_CASE_METHOD(IPCTestFixture, "XDGRuntimeDirExistsNoHyprDir", "[getSocketFolder]") {
  // Test case: XDG_RUNTIME_DIR exists but does not contain "hypr" directory
  // Arrange
  fs::path tempDir = fs::temp_directory_path() / "hypr_test/run/user/1000";
  fs::create_directories(tempDir);
  setenv("XDG_RUNTIME_DIR", tempDir.c_str(), 1);
  fs::path expectedPath = fs::path("/tmp") / "hypr" / instanceSig;

  // Act
  fs::path actualPath = getSocketFolder(instanceSig);

  // Assert expected result
  REQUIRE(actualPath == expectedPath);
}

TEST_CASE_METHOD(IPCTestFixture, "getSocket1Reply throws on no socket", "[getSocket1Reply]") {
  std::string request = "test_request";

  CHECK_THROWS(getSocket1Reply(request));
}
