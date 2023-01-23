#define CATCH_CONFIG_RUNNER
#include <glibmm.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#if __has_include(<catch2/catch_all.hpp>)
#include <catch2/catch_all.hpp>
#include <catch2/reporters/catch_reporter_tap.hpp>
#else
#include <catch2/catch.hpp>
#include <catch2/catch_reporter_tap.hpp>
#endif
#include <memory>

int main(int argc, char* argv[]) {
  Catch::Session session;
  Glib::init();

  session.applyCommandLine(argc, argv);
  const auto logger = spdlog::default_logger();
#if CATCH_VERSION_MAJOR >= 3
  for (const auto& spec : session.config().getReporterSpecs()) {
    const auto& reporter_name = spec.name();
#else
  {
    const auto& reporter_name = session.config().getReporterName();
#endif
    if (reporter_name == "tap") {
      spdlog::set_pattern("# [%l] %v");
    } else if (reporter_name == "compact") {
      logger->sinks().clear();
    } else {
      logger->sinks().assign({std::make_shared<spdlog::sinks::stderr_sink_st>()});
    }
  }

  return session.run();
}
