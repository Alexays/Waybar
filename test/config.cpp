#include "config.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("Load simple config", "[config]") {
  waybar::Config conf;
  conf.load("test/config/simple.json");

  SECTION("validate the config data") {
    auto& data = conf.getConfig();
    REQUIRE(data["layer"].asString() == "top");
    REQUIRE(data["height"].asInt() == 30);
  }
  SECTION("select configs for configured output") {
    auto configs = conf.getOutputConfigs("HDMI-0", "Fake HDMI output #0");
    REQUIRE(configs.size() == 1);
  }
  SECTION("select configs for missing output") {
    auto configs = conf.getOutputConfigs("HDMI-1", "Fake HDMI output #1");
    REQUIRE(configs.empty());
  }
}

TEST_CASE("Load config with multiple bars", "[config]") {
  waybar::Config conf;
  conf.load("test/config/multi.json");

  SECTION("select multiple configs #1") {
    auto data = conf.getOutputConfigs("DP-0", "Fake DisplayPort output #0");
    REQUIRE(data.size() == 3);
    REQUIRE(data[0]["layer"].asString() == "bottom");
    REQUIRE(data[0]["height"].asInt() == 20);
    REQUIRE(data[1]["layer"].asString() == "top");
    REQUIRE(data[1]["position"].asString() == "bottom");
    REQUIRE(data[1]["height"].asInt() == 21);
    REQUIRE(data[2]["layer"].asString() == "overlay");
    REQUIRE(data[2]["position"].asString() == "right");
    REQUIRE(data[2]["height"].asInt() == 23);
  }
  SECTION("select multiple configs #2") {
    auto data = conf.getOutputConfigs("HDMI-0", "Fake HDMI output #0");
    REQUIRE(data.size() == 2);
    REQUIRE(data[0]["layer"].asString() == "bottom");
    REQUIRE(data[0]["height"].asInt() == 20);
    REQUIRE(data[1]["layer"].asString() == "overlay");
    REQUIRE(data[1]["position"].asString() == "right");
    REQUIRE(data[1]["height"].asInt() == 23);
  }
  SECTION("select single config by output description") {
    auto data = conf.getOutputConfigs("HDMI-1", "Fake HDMI output #1");
    REQUIRE(data.size() == 1);
    REQUIRE(data[0]["layer"].asString() == "overlay");
    REQUIRE(data[0]["position"].asString() == "left");
    REQUIRE(data[0]["height"].asInt() == 22);
  }
}

TEST_CASE("Load simple config with include", "[config]") {
  waybar::Config conf;
  conf.load("test/config/include.json");

  SECTION("validate the config data") {
    auto& data = conf.getConfig();
    // config override behavior: preserve first included value
    REQUIRE(data["layer"].asString() == "top");
    REQUIRE(data["height"].asInt() == 30);
    // config override behavior: preserve value from the top config
    REQUIRE(data["position"].asString() == "top");
    // config override behavior: explicit null is still a value and should be preserved
    REQUIRE((data.isMember("nullOption") && data["nullOption"].isNull()));
  }
  SECTION("select configs for configured output") {
    auto configs = conf.getOutputConfigs("HDMI-0", "Fake HDMI output #0");
    REQUIRE(configs.size() == 1);
  }
  SECTION("select configs for missing output") {
    auto configs = conf.getOutputConfigs("HDMI-1", "Fake HDMI output #1");
    REQUIRE(configs.empty());
  }
}

TEST_CASE("Load multiple bar config with include", "[config]") {
  waybar::Config conf;
  conf.load("test/config/include-multi.json");

  SECTION("bar config with sole include") {
    auto data = conf.getOutputConfigs("OUT-0", "Fake output #0");
    REQUIRE(data.size() == 1);
    REQUIRE(data[0]["height"].asInt() == 20);
  }

  SECTION("bar config with output and include") {
    auto data = conf.getOutputConfigs("OUT-1", "Fake output #1");
    REQUIRE(data.size() == 1);
    REQUIRE(data[0]["height"].asInt() == 21);
  }

  SECTION("bar config with output override") {
    auto data = conf.getOutputConfigs("OUT-2", "Fake output #2");
    REQUIRE(data.size() == 1);
    REQUIRE(data[0]["height"].asInt() == 22);
  }

  SECTION("multiple levels of include") {
    auto data = conf.getOutputConfigs("OUT-3", "Fake output #3");
    REQUIRE(data.size() == 1);
    REQUIRE(data[0]["height"].asInt() == 23);
  }

  auto& data = conf.getConfig();
  REQUIRE(data.isArray());
  REQUIRE(data.size() == 4);
  REQUIRE(data[0]["output"].asString() == "OUT-0");
}
