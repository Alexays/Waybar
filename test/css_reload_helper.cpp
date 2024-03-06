#include "util/css_reload_helper.hpp"

#include <map>

#if __has_include(<catch2/catch_test_macros.hpp>)
#include <catch2/catch_test_macros.hpp>
#else
#include <catch2/catch.hpp>
#endif

class CssReloadHelperTest : public waybar::CssReloadHelper {
 public:
  CssReloadHelperTest() : CssReloadHelper("/tmp/waybar_test.css", [this]() { callback(); }) {}

  void callback() { m_callbackCounter++; }

 protected:
  std::string getFileContents(const std::string& filename) override {
    return m_fileContents[filename];
  }

  std::string findPath(const std::string& filename) override { return filename; }

  void setFileContents(const std::string& filename, const std::string& contents) {
    m_fileContents[filename] = contents;
  }

  int getCallbackCounter() const { return m_callbackCounter; }

 private:
  int m_callbackCounter{};
  std::map<std::string, std::string> m_fileContents;
};

TEST_CASE_METHOD(CssReloadHelperTest, "parse_imports", "[util][css_reload_helper]") {
  SECTION("no imports") {
    setFileContents("/tmp/waybar_test.css", "body { color: red; }");
    auto files = parseImports("/tmp/waybar_test.css");
    REQUIRE(files.size() == 1);
    CHECK(files[0] == "/tmp/waybar_test.css");
  }

  SECTION("single import") {
    setFileContents("/tmp/waybar_test.css", "@import 'test.css';");
    setFileContents("test.css", "body { color: red; }");
    auto files = parseImports("/tmp/waybar_test.css");
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 2);
    CHECK(files[0] == "/tmp/waybar_test.css");
    CHECK(files[1] == "test.css");
  }

  SECTION("multiple imports") {
    setFileContents("/tmp/waybar_test.css", "@import 'test.css'; @import 'test2.css';");
    setFileContents("test.css", "body { color: red; }");
    setFileContents("test2.css", "body { color: blue; }");
    auto files = parseImports("/tmp/waybar_test.css");
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 3);
    CHECK(files[0] == "/tmp/waybar_test.css");
    CHECK(files[1] == "test.css");
    CHECK(files[2] == "test2.css");
  }

  SECTION("nested imports") {
    setFileContents("/tmp/waybar_test.css", "@import 'test.css';");
    setFileContents("test.css", "@import 'test2.css';");
    setFileContents("test2.css", "body { color: red; }");
    auto files = parseImports("/tmp/waybar_test.css");
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 3);
    CHECK(files[0] == "/tmp/waybar_test.css");
    CHECK(files[1] == "test.css");
    CHECK(files[2] == "test2.css");
  }

  SECTION("circular imports") {
    setFileContents("/tmp/waybar_test.css", "@import 'test.css';");
    setFileContents("test.css", "@import 'test2.css';");
    setFileContents("test2.css", "@import 'test.css';");
    auto files = parseImports("/tmp/waybar_test.css");
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 3);
    CHECK(files[0] == "/tmp/waybar_test.css");
    CHECK(files[1] == "test.css");
    CHECK(files[2] == "test2.css");
  }

  SECTION("empty") {
    setFileContents("/tmp/waybar_test.css", "");
    auto files = parseImports("/tmp/waybar_test.css");
    REQUIRE(files.size() == 1);
    CHECK(files[0] == "/tmp/waybar_test.css");
  }

  SECTION("empty name") {
    auto files = parseImports("");
    REQUIRE(files.empty());
  }
}
