#pragma once

#include <linux/rfkill.h>

namespace waybar::util {

bool isDisabled(enum rfkill_type rfkill_type);

}  // namespace waybar::util
