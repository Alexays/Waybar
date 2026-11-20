#include <filesystem>
#include <fstream>
#include <random>
#include <string>

#include "util/hex_checker.hpp"

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

namespace fs = std::filesystem;

namespace {

// Creates a unique scratch directory and removes it when the test ends.
class TempDir {
 public:
  TempDir()
      : root_(fs::temp_directory_path() /
              ("waybar_8bit_test_" + std::to_string(std::random_device{}()))) {
    fs::create_directories(root_);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }

  // Writes a file below the scratch directory and returns its absolute path.
  std::string write(const std::string& rel, const std::string& content) const {
    const fs::path p = root_ / rel;
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out << content;
    return p.string();
  }

 private:
  fs::path root_;
};

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("transform_8bit_to_hex converts 8-bit colors", "[util][transform_8bit_to_rgba]") {
  TempDir dir;

  SECTION("8-bit color in the main file is converted") {
    const auto main = dir.write("style.css", "window { color: #11223344; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK(contains(result.css, "rgba(17,34,51,0.27)"));
    CHECK_FALSE(contains(result.css, "#11223344"));
  }

  SECTION("8-bit color in an imported file is converted") {
    dir.write("colors.css", "@define-color fg #aabbccdd;");
    const auto main = dir.write("style.css", "@import \"colors.css\";\nwindow { color: @fg; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK(contains(result.css, "rgba(170,187,204,0.87)"));
    CHECK_FALSE(contains(result.css, "#aabbccdd"));
    CHECK_FALSE(contains(result.css, "@import"));
  }

  SECTION("relative import is resolved from the importing file's directory") {
    // The 8-bit color only exists in themes/, so resolving "palette.css" from the
    // main file's directory would fail and leave the @import in place.
    dir.write("themes/palette.css", "@define-color bg #00ff0080;");
    dir.write("themes/dark.css", "@import \"palette.css\";");
    const auto main =
        dir.write("style.css", "@import \"themes/dark.css\";\nwindow { color: @bg; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK(contains(result.css, "rgba(0,255,0,0.50)"));
    CHECK_FALSE(contains(result.css, "@import"));
  }

  SECTION("no 8-bit colors anywhere reports not transformed") {
    dir.write("colors.css", "@define-color fg #aabbcc;");
    const std::string original = "@import \"colors.css\";\nwindow { color: #aabbcc; }";
    const auto main = dir.write("style.css", original);
    auto result = transform_8bit_to_hex(main);
    CHECK_FALSE(result.was_transformed);
    CHECK(result.css == original);
  }

  SECTION("non-file scheme imports are left as-is") {
    const std::string import = "@import url(\"resource:///com/example/style.css\");";
    const auto main = dir.write("style.css", import + "\nwindow { color: #11223344; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK(contains(result.css, import));
    CHECK(contains(result.css, "rgba(17,34,51,0.27)"));
  }

  SECTION("unreadable local import is left as-is") {
    const std::string import = "@import \"missing.css\";";
    const auto main = dir.write("style.css", import + "\nwindow { color: #11223344; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK(contains(result.css, import));
  }

  SECTION("import cycle terminates and still converts colors") {
    // a.css -> b.css -> a.css: the second a.css must stay as an @import.
    dir.write("a.css", "@import \"b.css\";\n/* a */");
    dir.write("b.css", "@import \"a.css\";\n@define-color bg #11223344;");
    const auto main = dir.write("style.css", "@import \"a.css\";\nwindow { color: @bg; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK(contains(result.css, "rgba(17,34,51,0.27)"));
    CHECK(contains(result.css, "@import \"a.css\";"));
  }
}

TEST_CASE("transform_8bit_to_hex does not inline imports inside comments",
          "[util][transform_8bit_to_rgba]") {
  TempDir dir;
  const std::string theme = "/* theme */ @define-color a #11223344; .leak{color:red;}";
  dir.write("th.css", theme);

  SECTION("commented-out import is left byte-for-byte") {
    const std::string original = "/* @import \"th.css\"; */ .real{color:#fff}";
    const auto main = dir.write("style.css", original);
    auto result = transform_8bit_to_hex(main);
    CHECK_FALSE(result.was_transformed);
    CHECK(result.css == original);
    CHECK_FALSE(contains(result.css, ".leak"));
  }

  SECTION("commented-out import does not inline, but real colors still convert") {
    const auto main =
        dir.write("style.css", "/* @import \"th.css\"; */ window { color: #11223344; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK(contains(result.css, "/* @import \"th.css\"; */"));
    CHECK_FALSE(contains(result.css, "@define-color"));
    CHECK_FALSE(contains(result.css, ".leak"));
    CHECK(contains(result.css, "rgba(17,34,51,0.27)"));
  }
}

TEST_CASE("transform_8bit_to_hex keeps relative url() paths valid",
          "[util][transform_8bit_to_rgba]") {
  TempDir dir;

  SECTION("relative url in an inlined file resolves against that file's directory") {
    dir.write("sub/inc.css",
              "@define-color c #11223344;\n.a { background-image: url(\"img.png\"); }");
    const auto main = dir.write("style.css", "@import \"sub/inc.css\";\nwindow { color: @c; }");
    auto result = transform_8bit_to_hex(main);
    const std::string expected = (fs::canonical(main).parent_path() / "sub" / "img.png").string();
    CHECK(result.was_transformed);
    CHECK(contains(result.css, "url(\"" + expected + "\")"));
    CHECK_FALSE(contains(result.css, "url(\"img.png\")"));
  }

  SECTION("relative url in the main file resolves against the main file's directory") {
    const auto main = dir.write("style.css",
                                ".a { background-image: url('bg.png'); }\n"
                                ".b { background-image: url(bg2.png); }\n"
                                "window { color: #11223344; }");
    auto result = transform_8bit_to_hex(main);
    const fs::path base = fs::canonical(main).parent_path();
    CHECK(result.was_transformed);
    CHECK(contains(result.css, "url('" + (base / "bg.png").string() + "')"));
    CHECK(contains(result.css, "url(\"" + (base / "bg2.png").string() + "\")"));
  }

  SECTION("absolute, scheme and data: urls are left unchanged") {
    const std::string urls =
        ".a { background-image: url(\"data:image/png;base64,AAAA\"); }\n"
        ".b { background-image: url(file:///x/y.png); }\n"
        ".c { background-image: url(\"/abs/z.png\"); }\n"
        ".d { background-image: url(http://example.com/q.png); }\n";
    const auto main = dir.write("style.css", urls + "window { color: #11223344; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK(contains(result.css, urls));
  }

  SECTION("url() inside a comment is left unchanged") {
    const auto main = dir.write("style.css", "/* url(\"c.png\") */ window { color: #11223344; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(contains(result.css, "/* url(\"c.png\") */"));
  }

  SECTION("relative url without an 8-bit color keeps was_transformed false") {
    const std::string original = ".a { background-image: url(\"img.png\"); }";
    const auto main = dir.write("style.css", original);
    auto result = transform_8bit_to_hex(main);
    CHECK_FALSE(result.was_transformed);
    CHECK(result.css == original);
  }
}

TEST_CASE("transform_8bit_to_hex strips a UTF-8 BOM from inlined files",
          "[util][transform_8bit_to_rgba]") {
  TempDir dir;
  const std::string bom = "\xEF\xBB\xBF";

  SECTION("BOM at the start of an inlined file is removed") {
    dir.write("colors.css", bom + "@define-color fg #aabbccdd;");
    const auto main = dir.write("style.css", "@import \"colors.css\";\nwindow { color: @fg; }");
    auto result = transform_8bit_to_hex(main);
    CHECK(result.was_transformed);
    CHECK_FALSE(contains(result.css, bom));
    CHECK(contains(result.css, "rgba(170,187,204,0.87)"));
  }
}
