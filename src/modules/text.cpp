#include "modules/text.hpp"

waybar::modules::Text::Text(const std::string& name,
  const Json::Value& config)
  : ALabel(config, "{}")
{
  label_.set_name("text-" + name);
  label_.set_markup(fmt::format(format_, name));
}
