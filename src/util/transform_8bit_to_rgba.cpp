#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
namespace fs = std::filesystem;

struct TransformResult {
  std::string css;
  bool was_transformed;
};

namespace {

// Nested @import depth limit; deeper imports are left as they are.
constexpr int kMaxImportDepth = 16;

// Same shape as IMPORT_REGEX in src/util/css_reload_helper.cpp.
const std::regex kImportPattern(R"(@import\s+(?:url\()?(?:"|')([^"')]+)(?:"|')\)?;)");

bool readFile(const fs::path& path, std::string& out) {
  std::error_code ec;
  if (!fs::is_regular_file(path, ec)) {
    return false;
  }
  std::ifstream f(path, std::ios::in | std::ios::binary);
  if (!f.is_open()) {
    return false;
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

// Local file path an @import target points to, or nullopt for other schemes
// (e.g. resource://). Relative paths are taken from base_dir.
std::optional<fs::path> localImportPath(const std::string& target, const fs::path& base_dir) {
  static const std::regex scheme_pattern(R"(^[A-Za-z][A-Za-z0-9+.-]*:)");
  std::string path = target;
  if (path.rfind("file://", 0) == 0) {
    path = path.substr(7);
  } else if (std::regex_search(path, scheme_pattern)) {
    return std::nullopt;
  }
  const fs::path p(path);
  return p.is_absolute() ? p : base_dir / p;
}

// Replaces every local @import in css with the contents of the file it names,
// recursively. active holds the canonical paths on the current import chain, so
// a cycle keeps its @import instead of recursing forever.
std::string inlineImports(const std::string& css, const fs::path& base_dir,
                          std::vector<fs::path>& active, int depth) {
  std::string out;
  auto last = css.cbegin();
  for (auto it = std::sregex_iterator(css.cbegin(), css.cend(), kImportPattern);
       it != std::sregex_iterator(); ++it) {
    const std::smatch& match = *it;
    out.append(last, match[0].first);
    last = match[0].second;

    std::string replacement = match.str(0);
    if (depth < kMaxImportDepth) {
      if (auto path = localImportPath(match[1].str(), base_dir)) {
        std::error_code ec;
        const fs::path canonical = fs::canonical(*path, ec);
        std::string content;
        if (!ec && std::find(active.begin(), active.end(), canonical) == active.end() &&
            readFile(canonical, content)) {
          active.push_back(canonical);
          replacement = inlineImports(content, canonical.parent_path(), active, depth + 1);
          active.pop_back();
        }
      }
    }
    out += replacement;
  }
  out.append(last, css.cend());
  return out;
}

}  // namespace

TransformResult transform_8bit_to_hex(const std::string& file_path) {
  std::ifstream f(file_path, std::ios::in | std::ios::binary);
  std::error_code ec;
  const auto size = fs::file_size(file_path, ec);
  if (ec || !f.is_open() || !f.good()) {
    throw std::runtime_error("Cannot open file: " + file_path);
  }
  std::string result(size, '\0');

  if (size == 0) {
    return {.css = result, .was_transformed = false};
  }

  f.read(result.data(), size);

  // Pull local @imports into the main text so their colors are converted too
  // and their relative paths no longer depend on where the CSS is loaded from.
  std::error_code canon_ec;
  fs::path main_path = fs::canonical(file_path, canon_ec);
  if (canon_ec) {
    main_path = file_path;
  }
  std::vector<fs::path> active{main_path};
  const std::string inlined = inlineImports(result, main_path.parent_path(), active, 0);

  static std::regex pattern(
      R"(\#([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2}))");
  std::string final_output;

  auto it = std::sregex_iterator(inlined.begin(), inlined.end(), pattern);
  auto eof = std::sregex_iterator();

  if (it == eof) {
    return {.css = result, .was_transformed = false};
  }

  std::smatch match;
  while (it != eof) {
    match = *it;

    final_output += match.prefix().str();

    int r = stoi(match[1].str(), nullptr, 16);
    int g = stoi(match[2].str(), nullptr, 16);
    int b = stoi(match[3].str(), nullptr, 16);
    double a = (stoi(match[4].str(), nullptr, 16) / 255.0);

    std::stringstream ss;
    ss << "rgba(" << r << "," << g << "," << b << "," << std::fixed << std::setprecision(2) << a
       << ")";
    final_output += ss.str();

    ++it;
  }

  final_output += match.suffix().str();

  return {.css = final_output, .was_transformed = true};
}
