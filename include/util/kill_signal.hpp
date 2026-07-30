#pragma once

#include <json/value.h>

#include <cstdint>

namespace waybar::util {

enum class KillSignalAction : std::uint8_t {
  TOGGLE,
  RELOAD,
  SHOW,
  HIDE,
  NOOP,
};
inline const std::map<std::string, KillSignalAction> userKillSignalActions = {
    {"TOGGLE", KillSignalAction::TOGGLE},
    {"RELOAD", KillSignalAction::RELOAD},
    {"SHOW", KillSignalAction::SHOW},
    {"HIDE", KillSignalAction::HIDE},
    {"NOOP", KillSignalAction::NOOP}};

inline const KillSignalAction SIGNALACTION_DEFAULT_SIGUSR1 = KillSignalAction::TOGGLE;
inline const KillSignalAction SIGNALACTION_DEFAULT_SIGUSR2 = KillSignalAction::RELOAD;

};  // namespace waybar::util
