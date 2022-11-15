#define CATCH_CONFIG_RUNNER
#include <glibmm.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include <catch2/catch_all.hpp>
#include <catch2/reporters/catch_reporter_tap.hpp>
#include <memory>

int main(int argc, char* argv[]) {
  Catch::Session session;
  Glib::init();

  session.applyCommandLine(argc, argv);
  const auto logger = spdlog::default_logger();
  for (const auto& spec : session.config().getReporterSpecs()) {
    if (spec.name() == "tap") {
      spdlog::set_pattern("# [%l] %v");
    } else if (spec.name() == "compact") {
      logger->sinks().clear();
    } else {
      logger->sinks().assign({std::make_shared<spdlog::sinks::stderr_sink_st>()});
    }
  }

  return session.run();
}
