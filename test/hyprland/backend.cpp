#include <cstdlib>
#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include "modules/hyprland/backend.hpp"

namespace fs = std::filesystem;
namespace hyprland = waybar::modules::hyprland;

class testRunListener : public Catch::EventListenerBase {
 public:
  using Catch::EventListenerBase::EventListenerBase;

  void testCaseStarting(Catch::TestCaseInfo const&) override {
    // TODO: reset state of module here
  }
};

CATCH_REGISTER_LISTENER(testRunListener)

TEST_CASE("GetSocketFolderTest", "[getSocketFolder]") {
  SECTION("XDGRuntimeDirExists") {
    // Test case: XDG_RUNTIME_DIR exists and contains "hypr" directory
    // Arrange
    std::cout << "Starting XDGRuntimeDirExists " << '\n';
    const char* instanceSig = "instance_sig";

    fs::path tempDir = fs::temp_directory_path() / "hypr_test/run/user/1000";
    std::cout << "Temp dir: " << tempDir << '\n';

    fs::path expectedPath = tempDir / "hypr" / instanceSig;
    std::cout << "Expected path: " << expectedPath << '\n';

    fs::create_directories(tempDir / "hypr" / instanceSig);

    setenv("XDG_RUNTIME_DIR", tempDir.c_str(), 1);

    // Act/*
    std::cout << "Getting socket folder" << '\n';
    fs::path actualPath = hyprland::getSocketFolder(instanceSig);

    // Assert expected result
    REQUIRE(actualPath == expectedPath);

    // Cleanup
    fs::remove_all(tempDir);

    std::cout << "Finishing XDGRuntimeDirExists " << '\n';
  }

  // TODO: properly clear state so we can actually test these....
  /* SECTION("XDGRuntimeDirDoesNotExist") { */
  /*   // Test case: XDG_RUNTIME_DIR does not exist */
  /*   // Arrange */
  /*   std::cout << "Starting XDGRuntimeDirDoesNotExist " << '\n'; */
  /*   const char* instanceSig = "instance_sig"; */
  /**/
  /*   std::cout << "Current XDG_RUNTIME_DIR: " << getenv("XDG_RUNTIME_DIR") << '\n'; */
  /**/
  /*   unsetenv("XDG_RUNTIME_DIR"); */
  /**/
  /*   std::cout << "New XDG_RUNTIME_DIR: " << getenv("XDG_RUNTIME_DIR") << '\n'; */
  /**/
  /*   // Act */
  /*   fs::path actualPath = hyprland::getSocketFolder(instanceSig); */
  /**/
  /*   // Assert expected result */
  /*   fs::path expectedPath = fs::path("/tmp") / "hypr" / instanceSig; */
  /*   REQUIRE(actualPath == expectedPath); */
  /**/
  /*   // Cleanup */
  /*   std::cout << "Finishing XDGRuntimeDirDoesNotExist " << '\n'; */
  /* } */
  /**/
  /* SECTION("XDGRuntimeDirExistsNoHyprDir") { */
  /*   // Test case: XDG_RUNTIME_DIR exists but does not contain "hypr" directory */
  /*   // Arrange */
  /*   std::cout << "Starting XDGRuntimeDirExistsNoHyprDir " << '\n'; */
  /**/
  /*   const char* instanceSig = "instance_sig"; */
  /**/
  /*   fs::path tempDir = fs::temp_directory_path() / "hypr_test/run/user/1000"; */
  /*   std::cout << "Temp dir: " << tempDir << '\n'; */
  /**/
  /*   fs::create_directories(tempDir); */
  /**/
  /*   setenv("XDG_RUNTIME_DIR", tempDir.c_str(), 1); */
  /**/
  /*   std::cout << "Current XDG_RUNTIME_DIR: " << getenv("XDG_RUNTIME_DIR") << '\n'; */
  /**/
  /*   // Act */
  /*   fs::path actualPath = hyprland::getSocketFolder(instanceSig); */
  /**/
  /*   // Assert expected result */
  /*   fs::path expectedPath = fs::path("/tmp") / "hypr" / instanceSig; */
  /*   std::cout << "Expected path: " << expectedPath << '\n'; */
  /**/
  /*   REQUIRE(actualPath == expectedPath); */
  /**/
  /*   // Cleanup */
  /*   fs::remove_all(tempDir); */
  /**/
  /*   std::cout << "Finishing XDGRuntimeDirExistsNoHyprDir " << '\n'; */
  /* } */
}
