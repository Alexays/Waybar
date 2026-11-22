#pragma once

#include <fmt/args.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "ALabel.hpp"
#include "util/sleeper_thread.hpp"

namespace waybar::modules {

class CpuUsage : public ALabel {
 public:
  CpuUsage(const std::string&, const Json::Value&);

  auto update() -> void override;

  // Usage computed from two CPU counter snapshots. After a topology change,
  // cores_pending_ is set and only the total is available if it could be computed.
  struct Sample {
    // Total usage comes first. Offline cores have no value.
    std::vector<std::optional<uint16_t>> usage_;
    bool cores_pending_ = false;

    int totalUsage() const { return usage_.empty() ? 0 : usage_.front().value_or(0); }
    std::string defaultTooltip() const;
  };

  // Adds CPU usage fields to a fmt store and caches icons as they are rendered.
  // Ensure you keep this object alive while formatting the label and tooltip for one
  // update.
  class FormatArgs {
   public:
    // Borrows the module, sample, and active state for the duration of formatting.
    FormatArgs(ALabel& label, const Sample& sample, const std::string& state)
        : label_(label), sample_(sample), state_(state) {}

    // Appends the aggregate usage and icon arguments.
    void addTotal(fmt::dynamic_format_arg_store<fmt::format_context>& store);

    // Appends the usageN/iconN pairs in core order, followed by the combined icons argument.
    void addCores(fmt::dynamic_format_arg_store<fmt::format_context>& store);

   private:
    const std::string& icon(uint16_t usage);
    const std::string& coreIcons();

    ALabel& label_;
    const Sample& sample_;
    const std::string& state_;
    std::unordered_map<uint16_t, std::string> icons_;
    std::optional<std::string> core_icons_;
  };

  // Computes usage from the difference between snapshots and updates prev_times
  // with the current snapshot. Each snapshot holds (idle, total) counters for
  // the aggregate CPU first, followed by each logical CPU. Pass an empty vector
  // on the first call to take two snapshots 100 ms apart.
  static Sample getCpuUsage(std::vector<std::tuple<std::size_t, std::size_t>>& prev_times);

 private:
  static std::vector<std::tuple<std::size_t, std::size_t>> parseCpuinfo();

  std::vector<std::tuple<std::size_t, std::size_t>> prev_times_;

  util::SleeperThread thread_;
};

}  // namespace waybar::modules
