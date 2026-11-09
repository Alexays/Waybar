#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <optional>
#include <string>
#include <system_error>
#include <thread>

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
  using hyprland::IPC::isLuaConfigProvider;
  using hyprland::IPC::isLuaProtocol;
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

TEST_CASE("Event listener reconnects when Hyprland drops it", "[socketListener]") {
  struct Handler : hyprland::EventHandler {
    void onEvent(const std::string&) override { ++events; }
    void onReconnect() override { ++reconnects; }
    std::atomic<int> events = 0;
    std::atomic<int> reconnects = 0;
  } handler;

  constexpr auto instanceSig = "instance_sig";
  const fs::path tempDir = fs::temp_directory_path() / "hypr_test/run/user/1000";
  std::error_code ec;
  fs::remove_all(tempDir, ec);
  fs::create_directories(tempDir / "hypr" / instanceSig);
  setenv("XDG_RUNTIME_DIR", tempDir.c_str(), 1);
  setenv("HYPRLAND_INSTANCE_SIGNATURE", instanceSig, 1);
  IPCTestHelper::resetSocketFolder();

  sockaddr_un addr = {};
  addr.sun_family = AF_UNIX;
  const auto socketPath = tempDir / "hypr" / instanceSig / ".socket2.sock";
  strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
  const int server = socket(AF_UNIX, SOCK_STREAM, 0);
  REQUIRE(server != -1);
  REQUIRE(bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
  REQUIRE(listen(server, 1) == 0);

  const auto acceptClient = [server] {
    pollfd pfd = {server, POLLIN, 0};
    return poll(&pfd, 1, 10000) == 1 ? accept(server, nullptr, nullptr) : -1;
  };
  const auto sendEvent = [&handler](int client) {
    const int seen = handler.events;
    REQUIRE(write(client, "test>>\n", 7) == 7);
    for (int i = 0; i < 1000 && handler.events == seen; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(handler.events == seen + 1);
  };

  {
    IPCTestHelper ipc;
    // registered twice, must still be told only once about a reconnect
    ipc.registerForIPC("test", &handler);
    ipc.registerForIPC("other", &handler);

    int client = acceptClient();
    REQUIRE(client != -1);
    sendEvent(client);
    REQUIRE(handler.reconnects == 0);
    close(client);

    client = acceptClient();
    REQUIRE(client != -1);
    sendEvent(client);
    REQUIRE(handler.reconnects == 1);
    close(client);
  }

  close(server);
  unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  fs::remove_all(tempDir, ec);
}

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

TEST_CASE("buildLuaDispatch quotes the argument as a Lua string", "[buildLuaDispatch]") {
  SECTION("workspace") {
    auto result = IPCTestHelper::buildLuaDispatch("workspace", "name:a\"b\\c\nd");
    REQUIRE(result == "/dispatch hl.dsp.focus({ workspace = \"name:a\\\"b\\\\c\\010d\" })");
  }
  SECTION("focusworkspaceoncurrentmonitor") {
    auto result = IPCTestHelper::buildLuaDispatch("focusworkspaceoncurrentmonitor",
                                                  "name:x\" .. tostring(6 * 7) .. \"");
    REQUIRE(result ==
            "/dispatch hl.dsp.focus({ workspace = \"name:x\\\" .. tostring(6 * 7) .. \\\"\", "
            "on_current_monitor = true })");
  }
  SECTION("togglespecialworkspace") {
    auto result = IPCTestHelper::buildLuaDispatch("togglespecialworkspace", "x\") .. (\"");
    REQUIRE(result == "/dispatch hl.dsp.workspace.toggle_special(\"x\\\") .. (\\\"\")");
  }
  SECTION("unknown dispatcher fallback") {
    auto result = IPCTestHelper::buildLuaDispatch("unknown_dispatcher", "a\"b");
    REQUIRE(result == "/dispatch hl.dsp.unknown_dispatcher(\"a\\\"b\")");
  }
}

TEST_CASE("dispatch throws when Hyprland is not running", "[dispatch]") {
  unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  IPCTestHelper::resetSocketFolder();
  IPCTestHelper::resetLuaProtocolDetection();

  CHECK_THROWS(hyprland::IPC::dispatch("workspace", "1"));
}

TEST_CASE("isLuaConfigProvider reads the config manager Hyprland loaded", "[isLuaConfigProvider]") {
  SECTION("realistic systeminfo reply reports the Lua manager") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider(kSystemInfoLua) == true);
  }

  SECTION("lua") { REQUIRE(IPCTestHelper::isLuaConfigProvider("configProvider: lua\n") == true); }

  // "hyprlang" is what Hyprland actually emits for the legacy manager, per
  // Config::typeToString in src/config/ConfigManager.cpp.
  SECTION("hyprlang") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider("configProvider: hyprlang\n") == false);
  }

  // A >= 0.55 instance started with a traditional hyprland.conf keeps the
  // legacy parser, which is exactly the case the version heuristic got wrong.
  SECTION("legacy manager inside a full reply") {
    std::string info{kSystemInfoLua};
    info.replace(info.find("configProvider: lua"), std::strlen("configProvider: lua"),
                 "configProvider: hyprlang");
    REQUIRE(IPCTestHelper::isLuaConfigProvider(info) == false);
  }

  SECTION("an unrecognised manager is not treated as Lua") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider("configProvider: something-else\n") == false);
  }

  // The field ships together with the Lua config manager (0.55), so replies
  // without it come from instances that only speak the legacy protocol.
  SECTION("absent field means a pre-Lua instance, hence legacy") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider("Hyprland 0.41.2\nTag: v0.41.2\n") == false);
  }

  SECTION("empty reply") { REQUIRE(IPCTestHelper::isLuaConfigProvider("") == false); }
}

TEST_CASE("isLuaConfigProvider tolerates formatting variations", "[isLuaConfigProvider]") {
  SECTION("tab separator") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider("configProvider:\tlua\n") == true);
  }

  SECTION("extra spaces") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider("configProvider:    lua\n") == true);
  }

  SECTION("CRLF line ending") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider("configProvider: lua\r\n") == true);
  }

  SECTION("last line without a trailing newline") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider("plugins:\nconfigProvider: lua") == true);
  }

  SECTION("empty value is not Lua") {
    REQUIRE(IPCTestHelper::isLuaConfigProvider("configProvider:\n") == false);
  }
}

TEST_CASE("isLuaProtocol assumes legacy when Hyprland is not reachable", "[isLuaProtocol]") {
  // getSocket1Reply throws; detection must degrade to legacy instead of
  // propagating and breaking the click.
  unsetenv("HYPRLAND_INSTANCE_SIGNATURE");
  IPCTestHelper::resetSocketFolder();
  IPCTestHelper::resetLuaProtocolDetection();

  REQUIRE(IPCTestHelper::isLuaProtocol() == false);

  // Cleanup: drop the cached result so other tests aren't affected
  IPCTestHelper::resetLuaProtocolDetection();
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
