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
