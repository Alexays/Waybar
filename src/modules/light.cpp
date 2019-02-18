#include "modules/light.hpp"

waybar::modules::Light::Light(const std::string& id, const Json::Value &config)
    : ALabel(config, "{brightness}%", 5),
      brightness_level_(100),
      scrolling_(false)
{
  label_.set_name("light");
  if (!id.empty()) {
    label_.get_style_context()->add_class(id);
  }

  delayWorker();

  event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box_.signal_scroll_event().connect(
    sigc::mem_fun(*this, &Light::handleScroll));
}

auto waybar::modules::Light::update() -> void
{
  auto str = fmt::format(format_, fmt::arg("brightness", brightness_level_));
  label_.set_markup(str);
  label_.set_tooltip_text(str); // TODO: Tooltip text

  if (scrolling_) {
    scrolling_ = false;
  }
}

void waybar::modules::Light::delayWorker()
{
  thread_ = [this] {
    updateBrightness();
    thread_.sleep_for(interval_);
  };
}

bool waybar::modules::Light::handleScroll(GdkEventScroll *e) {
  bool direction_up = false;

  // Avoid concurrent scroll event
  if (scrolling_) {
    return false;
  }
  scrolling_ = true;

  uint16_t change = config_["scroll-step"].isUInt() ? config_["scroll-step"].asUInt() : 5;

  if (e->direction == GDK_SCROLL_UP) {
    direction_up = true;
  }
  if (e->direction == GDK_SCROLL_DOWN) {
    direction_up = false;
  }
  if (e->direction == GDK_SCROLL_SMOOTH) {
    gdouble delta_x, delta_y;
    gdk_event_get_scroll_deltas(reinterpret_cast<const GdkEvent *>(e), &delta_x, &delta_y);
    if (delta_y < 0) {
      direction_up = true;
    } else if (delta_y > 0) {
      direction_up = false;
    }
  }

  if (direction_up) {
    if (brightness_level_ + 1 < 100)  {
      auto res = waybar::util::command::exec(fmt::format(cmd_increase_, change));
    }
  } else {
    if (brightness_level_ - 1 > 0) {
      auto res = waybar::util::command::exec(fmt::format(cmd_decrease_, change));
    }
  }
  updateBrightness();
  return true;
}

void waybar::modules::Light::updateBrightness()
{
  float temp_float;
  auto res = waybar::util::command::exec(cmd_get_);
  if (res.exit_code == 0) {
    // Convert float to uint8_t
    sscanf(res.out.c_str(), "%f", &temp_float);
    brightness_level_ = (uint8_t) (temp_float + 0.5);
    dp.emit();
  }
}
