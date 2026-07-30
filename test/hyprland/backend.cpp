#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <cstring>
#include <optional>
#include <string>
#include <system_error>

#include "modules/hyprland/backend.hpp"

namespace fs = std::filesystem;
namespace hyprland = waybar::modules::hyprland;

namespace {
class IPCTestHelper : public hyprland::IPC {
 public:
  static void resetSocketFolder() { socketFolder_.clear(); }
  static void resetLuaProtocolDetection() { s_luaProtocolDetected_.reset(); }
  static void setLuaProtocolDetected(bool value) { s_luaProtocolDetected_ = value; }
  using hyprland::IPC::buildLuaDispatch;
  using hyprland::IPC::isLuaProtocol;
  using hyprland::IPC::luaProtocolFromSystemInfo;
  using hyprland::IPC::parseConfigProvider;
};

// Trimmed but otherwise verbatim "systeminfo" reply from Hyprland 0.56.1.
constexpr auto kSystemInfoLua = R"(
Hyprland 0.56.1 built from branch v0.56.1 at commit deadbeef clean.
Date: Mon Jul 27 16:33:49 2026
Tag: v0.56.1, commits: 7643

Libraries:
Hyprutils: built against 0.14.0, system has 0.14.0

os-release: Fedora Linux 43

plugins:
  no plugins loaded

configProvider: lua
)";

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

// --- Tests for new Lua IPC dispatch functions ---

TEST_CASE("buildLuaDispatch workspace", "[buildLuaDispatch]") {
  SECTION("numeric workspace") {
    auto result = IPCTestHelper::buildLuaDispatch("workspace", "1");
    REQUIRE(result == "/dispatch hl.dsp.focus({ workspace = \"1\" })");
  }
  SECTION("named workspace") {
    auto result = IPCTestHelper::buildLuaDispatch("workspace", "name:term");
    REQUIRE(result == "/dispatch hl.dsp.focus({ workspace = \"name:term\" })");
  }
  SECTION("relative workspace") {
    auto result = IPCTestHelper::buildLuaDispatch("workspace", "e+1");
    REQUIRE(result == "/dispatch hl.dsp.focus({ workspace = \"e+1\" })");
  }
}

TEST_CASE("buildLuaDispatch focusworkspaceoncurrentmonitor", "[buildLuaDispatch]") {
  auto result =
      IPCTestHelper::buildLuaDispatch("focusworkspaceoncurrentmonitor", "3");
  REQUIRE(
      result ==
      "/dispatch hl.dsp.focus({ workspace = \"3\", on_current_monitor = true })");
}

TEST_CASE("buildLuaDispatch togglespecialworkspace", "[buildLuaDispatch]") {
  SECTION("with name") {
    auto result =
        IPCTestHelper::buildLuaDispatch("togglespecialworkspace", "scratchpad");
    REQUIRE(result ==
            "/dispatch hl.dsp.workspace.toggle_special(\"scratchpad\")");
  }
  SECTION("empty arg") {
    auto result =
        IPCTestHelper::buildLuaDispatch("togglespecialworkspace", "");
    REQUIRE(result == "/dispatch hl.dsp.workspace.toggle_special()");
  }
}

TEST_CASE("buildLuaDispatch unknown dispatcher fallback", "[buildLuaDispatch]") {
  auto result =
      IPCTestHelper::buildLuaDispatch("unknown_dispatcher", "some_arg");
  REQUIRE(result ==
          "/dispatch hl.dsp.unknown_dispatcher(\"some_arg\")");
}

TEST_CASE("dispatch throws when Hyprland is not running", "[dispatch]") {
  unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  IPCTestHelper::resetSocketFolder();
  IPCTestHelper::resetLuaProtocolDetection();

  CHECK_THROWS(hyprland::IPC::dispatch("workspace", "1"));
}

TEST_CASE("parseConfigProvider reads the config manager Hyprland loaded", "[parseConfigProvider]") {
  SECTION("realistic systeminfo reply reports the Lua manager") {
    REQUIRE(IPCTestHelper::parseConfigProvider(kSystemInfoLua) == true);
  }

  SECTION("lua") { REQUIRE(IPCTestHelper::parseConfigProvider("configProvider: lua\n") == true); }

  SECTION("legacy") {
    REQUIRE(IPCTestHelper::parseConfigProvider("configProvider: legacy\n") == false);
  }

  // A >= 0.54 instance started with a traditional hyprland.conf keeps the
  // legacy parser, which is exactly the case the version heuristic got wrong.
  SECTION("legacy manager inside a full reply") {
    std::string info{kSystemInfoLua};
    info.replace(info.find("configProvider: lua"), std::strlen("configProvider: lua"),
                 "configProvider: legacy");
    REQUIRE(IPCTestHelper::parseConfigProvider(info) == false);
  }

  SECTION("an unrecognised manager is not treated as Lua") {
    REQUIRE(IPCTestHelper::parseConfigProvider("configProvider: something-else\n") == false);
  }
}

TEST_CASE("parseConfigProvider tolerates formatting variations", "[parseConfigProvider]") {
  SECTION("tab separator") {
    REQUIRE(IPCTestHelper::parseConfigProvider("configProvider:\tlua\n") == true);
  }

  SECTION("extra spaces") {
    REQUIRE(IPCTestHelper::parseConfigProvider("configProvider:    lua\n") == true);
  }

  SECTION("CRLF line ending") {
    REQUIRE(IPCTestHelper::parseConfigProvider("configProvider: lua\r\n") == true);
  }

  SECTION("last line without a trailing newline") {
    REQUIRE(IPCTestHelper::parseConfigProvider("plugins:\nconfigProvider: lua") == true);
  }

  SECTION("empty value is not Lua") {
    REQUIRE(IPCTestHelper::parseConfigProvider("configProvider:\n") == false);
  }
}

TEST_CASE("parseConfigProvider returns nullopt when the field is absent", "[parseConfigProvider]") {
  // Hyprland versions predating the Lua config manager do not report the field;
  // callers must fall back to version detection rather than assume legacy here.
  SECTION("older systeminfo reply") {
    REQUIRE(IPCTestHelper::parseConfigProvider("Hyprland 0.41.2\nTag: v0.41.2\n") == std::nullopt);
  }

  SECTION("empty reply") { REQUIRE(IPCTestHelper::parseConfigProvider("") == std::nullopt); }
}

TEST_CASE("luaProtocolFromSystemInfo returns nullopt when Hyprland is not running",
          "[luaProtocolFromSystemInfo]") {
  // getSocket1Reply throws; detection must degrade to the version fallback
  // instead of propagating and breaking the click.
  unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  IPCTestHelper::resetSocketFolder();

  REQUIRE(IPCTestHelper::luaProtocolFromSystemInfo() == std::nullopt);
}

TEST_CASE("isLuaProtocol uses cached value and avoids socket call",
          "[isLuaProtocol]") {
  unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  IPCTestHelper::resetSocketFolder();

  SECTION("cached false") {
    IPCTestHelper::setLuaProtocolDetected(false);
    // Should return false without throwing (no socket call needed)
    REQUIRE(IPCTestHelper::isLuaProtocol() == false);
  }

  SECTION("cached true") {
    IPCTestHelper::setLuaProtocolDetected(true);
    // Should return true without throwing (no socket call needed)
    REQUIRE(IPCTestHelper::isLuaProtocol() == true);
  }

  // Cleanup: reset detection so other tests aren't affected
  IPCTestHelper::resetLuaProtocolDetection();
}
