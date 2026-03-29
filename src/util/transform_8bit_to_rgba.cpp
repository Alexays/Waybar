#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
namespace fs = std::filesystem;
std::string transform_8bit_to_hex(std::string file_path) {
  std::ifstream f(file_path, std::ios::in | std::ios::binary);
  const auto size = fs::file_size(file_path);
  std::string result(size, '\0');
  f.read(result.data(), size);

  std::regex pattern(
      R"((?:\#([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2})))");
  std::string final_output;

  auto it = std::sregex_iterator(result.begin(), result.end(), pattern);
  auto eof = std::sregex_iterator();
  std::smatch match = *it;

  while (it != eof) {

    final_output += match.prefix().str();

    int r = stoi(match[1].str(), nullptr, 16);
    int g = stoi(match[2].str(), nullptr, 16);
    int b = stoi(match[3].str(), nullptr, 16);
    double a = (stoi(match[4].str(), nullptr, 16) / 255.0);

    std::stringstream ss;
    ss << "rgba(" << r << "," << g << "," << b << "," << std::fixed
       << std::setprecision(2) << a << ")";
    final_output += ss.str();

    ++it;
  }

  final_output += match.suffix().str();

  return final_output;
}
