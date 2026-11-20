#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
namespace fs = std::filesystem;

struct TransformResult {
  std::string css;
  bool was_transformed;
};

namespace {

// Nested @import depth limit; deeper imports are left as they are.
constexpr int kMaxImportDepth = 16;

// Same pattern as IMPORT_REGEX in src/util/css_reload_helper.cpp, which the reload
// watcher uses to find imports. Change both together.
const std::regex kImportPattern(R"(@import\s+(?:url\()?(?:"|')([^"')]+)(?:"|')\)?;)");

// url(...) with a double-quoted, single-quoted or unquoted target.
const std::regex kUrlPattern(R"re([uU][rR][lL]\(\s*(?:"([^"]*)"|'([^']*)'|([^"'()\s]*))\s*\))re");

const std::regex kSchemePattern(R"(^[A-Za-z][A-Za-z0-9+.-]*:)");

const std::regex kColorPattern(
    R"(\#([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2}))");

const std::string kUtf8Bom = "\xEF\xBB\xBF";

// What each byte of a stylesheet is: CSS code, a /* */ comment, or a quoted string.
enum class Span : unsigned char { kCode, kComment, kString };

// A replacement of css[begin, end) with text.
struct Edit {
  size_t begin;
  size_t end;
  std::string text;
};

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

// Marks the bytes of comments and quoted strings. A string ends at its closing quote or
// at a newline, as in CSS.
std::vector<Span> classifySpans(const std::string& css) {
  std::vector<Span> spans(css.size(), Span::kCode);
  size_t i = 0;
  while (i < css.size()) {
    size_t end = i + 1;
    Span kind = Span::kCode;
    if (css.compare(i, 2, "/*") == 0) {
      const size_t close = css.find("*/", i + 2);
      end = (close == std::string::npos) ? css.size() : close + 2;
      kind = Span::kComment;
    } else if (css[i] == '"' || css[i] == '\'') {
      const char quote = css[i];
      end = i + 1;
      while (end < css.size() && css[end] != quote && css[end] != '\n') {
        end += (css[end] == '\\' && end + 1 < css.size()) ? 2 : 1;
      }
      if (end < css.size() && css[end] == quote) {
        ++end;
      }
      kind = Span::kString;
    }
    if (kind != Span::kCode) {
      std::fill(spans.begin() + static_cast<std::ptrdiff_t>(i),
                spans.begin() + static_cast<std::ptrdiff_t>(end), kind);
    }
    i = end;
  }
  return spans;
}

// Local file path an @import target points to, or nullopt for other schemes
// (e.g. resource://). Relative paths are taken from base_dir.
std::optional<fs::path> localImportPath(const std::string& target, const fs::path& base_dir) {
  std::string path = target;
  if (path.rfind("file://", 0) == 0) {
    path = path.substr(7);
  } else if (std::regex_search(path, kSchemePattern)) {
    return std::nullopt;
  }
  const fs::path p(path);
  return p.is_absolute() ? p : base_dir / p;
}

bool overlapsAny(const std::vector<std::pair<size_t, size_t>>& ranges, size_t begin, size_t end) {
  return std::any_of(ranges.begin(), ranges.end(),
                     [&](const auto& r) { return begin < r.second && r.first < end; });
}

// True when css[begin, begin + 'url' ...) is the url() of an @import statement.
bool isImportUrl(const std::string& css, size_t begin) {
  static const std::string kKeyword = "@import";
  size_t i = begin;
  while (i > 0 && std::isspace(static_cast<unsigned char>(css[i - 1]))) {
    --i;
  }
  return i >= kKeyword.size() && css.compare(i - kKeyword.size(), kKeyword.size(), kKeyword) == 0;
}

// Absolute form of a relative url() target, resolved against base_dir. Nullopt for
// targets that stay as they are: empty, absolute, fragment-only, or any scheme.
std::optional<std::string> absoluteUrlTarget(const std::string& target, const fs::path& base_dir) {
  if (target.empty() || target[0] == '/' || target[0] == '#' ||
      std::regex_search(target, kSchemePattern)) {
    return std::nullopt;
  }
  return (base_dir / target).lexically_normal().string();
}

// Quotes value as a CSS string using the given quote character.
std::string quoteCss(const std::string& value, char quote) {
  std::string out(1, quote);
  for (const char c : value) {
    if (c == quote || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  out += quote;
  return out;
}

// Replaces every local @import in css with the contents of the file it names,
// recursively, and makes the relative url() targets of css absolute against
// base_dir. active holds the canonical paths on the current import chain, so a cycle
// keeps its @import instead of recursing forever.
std::string inlineImports(const std::string& css, const fs::path& base_dir,
                          std::vector<fs::path>& active, int depth) {
  const std::vector<Span> spans = classifySpans(css);
  std::vector<Edit> edits;
  std::vector<std::pair<size_t, size_t>> import_spans;

  for (auto it = std::sregex_iterator(css.begin(), css.end(), kImportPattern);
       it != std::sregex_iterator(); ++it) {
    const std::smatch& match = *it;
    const size_t begin = static_cast<size_t>(match.position(0));
    const size_t end = begin + static_cast<size_t>(match.length(0));
    // An @import inside a comment or a string is plain text and stays as it is.
    if (spans[begin] != Span::kCode) {
      continue;
    }
    import_spans.emplace_back(begin, end);
    if (depth >= kMaxImportDepth) {
      continue;
    }
    const auto path = localImportPath(match[1].str(), base_dir);
    if (!path) {
      continue;
    }
    std::error_code ec;
    const fs::path canonical = fs::canonical(*path, ec);
    std::string content;
    if (ec || std::find(active.begin(), active.end(), canonical) != active.end() ||
        !readFile(canonical, content)) {
      continue;
    }
    // A BOM is only valid at the very start of a stylesheet; inlined, it would sit mid-stream.
    if (content.starts_with(kUtf8Bom)) {
      content.erase(0, kUtf8Bom.size());
    }
    active.push_back(canonical);
    edits.push_back(
        {begin, end, inlineImports(content, canonical.parent_path(), active, depth + 1)});
    active.pop_back();
  }

  for (auto it = std::sregex_iterator(css.begin(), css.end(), kUrlPattern);
       it != std::sregex_iterator(); ++it) {
    const std::smatch& match = *it;
    const size_t begin = static_cast<size_t>(match.position(0));
    const size_t end = begin + static_cast<size_t>(match.length(0));
    if (spans[begin] != Span::kCode || isImportUrl(css, begin) ||
        overlapsAny(import_spans, begin, end)) {
      continue;
    }
    if (begin > 0 && (css[begin - 1] == '_' || css[begin - 1] == '-' ||
                      std::isalnum(static_cast<unsigned char>(css[begin - 1])))) {
      continue;  // part of a longer name, such as "myurl("
    }
    if (std::any_of(spans.begin() + static_cast<std::ptrdiff_t>(begin),
                    spans.begin() + static_cast<std::ptrdiff_t>(end),
                    [](Span s) { return s == Span::kComment; })) {
      continue;
    }

    char quote = '"';
    std::string target = match[3].str();
    if (match[1].matched) {
      target = match[1].str();
    } else if (match[2].matched) {
      target = match[2].str();
      quote = '\'';
    }
    const auto absolute = absoluteUrlTarget(target, base_dir);
    if (absolute) {
      edits.push_back({begin, end, "url(" + quoteCss(*absolute, quote) + ")"});
    }
  }

  if (edits.empty()) {
    return css;
  }
  std::sort(edits.begin(), edits.end(),
            [](const Edit& a, const Edit& b) { return a.begin < b.begin; });
  std::string out;
  size_t last = 0;
  for (const Edit& edit : edits) {
    if (edit.begin < last) {
      continue;
    }
    out.append(css, last, edit.begin - last);
    out += edit.text;
    last = edit.end;
  }
  out.append(css, last, std::string::npos);
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

  // Pull local @imports into the main text so their colors are converted too, and
  // make relative url() paths absolute so they no longer depend on the load location.
  std::error_code canon_ec;
  fs::path main_path = fs::canonical(file_path, canon_ec);
  if (canon_ec) {
    main_path = file_path;
  }
  std::vector<fs::path> active{main_path};
  const std::string inlined = inlineImports(result, main_path.parent_path(), active, 0);

  std::string final_output;

  auto it = std::sregex_iterator(inlined.begin(), inlined.end(), kColorPattern);
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
