#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

bool has_8bit_hex(std::string file_path) {
  std::ifstream f(file_path, std::ios::in | std::ios::binary);
  const auto size = fs::file_size(file_path);
  std::string result(size, '\0');
  f.read(result.data(), size);
  std::regex pattern(
      R"((?:\#([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2})))");
  return std::regex_search(result, pattern);
}
