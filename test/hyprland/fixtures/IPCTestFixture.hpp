#include "modules/hyprland/backend.hpp"

namespace fs = std::filesystem;
namespace hyprland = waybar::modules::hyprland;

class IPCTestFixture : public hyprland::IPC {
 public:
  IPCTestFixture() : IPC() { IPC::socketFolder_ = ""; }
  ~IPCTestFixture() { fs::remove_all(tempDir); }

 protected:
  const char* instanceSig = "instance_sig";
  fs::path tempDir = fs::temp_directory_path() / "hypr_test";

 private:
};

class IPCMock : public IPCTestFixture {
 public:
  // Mock getSocket1Reply to return an empty string
  static std::string getSocket1Reply(const std::string& rq) { return ""; }

 protected:
  const char* instanceSig = "instance_sig";
};
