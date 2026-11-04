#include "AIconLabel.hpp"

#include <gdkmm/pixbuf.h>
#include <glibmm/fileutils.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace waybar {

AIconLabel::AIconLabel(const Json::Value& config, const std::string& name, const std::string& id,
                       const std::string& format, uint16_t interval, bool ellipsize,
                       bool enable_click, bool enable_scroll)
    : ALabel(config, name, id, format, interval, ellipsize, enable_click, enable_scroll) {
  if (config["icon-size"].isUInt()) {
    app_icon_size_ = config["icon-size"].asUInt();
  }
  image_.set_pixel_size(app_icon_size_);
  event_box_.remove();
  label_.unset_name();
  label_.get_style_context()->remove_class(MODULE_CLASS);
  box_.get_style_context()->add_class(MODULE_CLASS);
  if (!id.empty()) {
    label_.get_style_context()->remove_class(id);
    box_.get_style_context()->add_class(id);
  }

  int rot = 0;

  if (config_["rotate"].isUInt()) {
    rot = config["rotate"].asUInt() % 360;
    if ((rot % 90) != 00) rot = 0;
    rot /= 90;
  }

  if ((rot % 2) == 0)
    box_.set_orientation(Gtk::Orientation::ORIENTATION_HORIZONTAL);
  else
    box_.set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);
  box_.set_name(name);

  int spacing = config_["icon-spacing"].isInt() ? config_["icon-spacing"].asInt() : 8;
  box_.set_spacing(spacing);

  bool swap_icon_label = false;
  if (config_["swap-icon-label"].isNull()) {
  } else if (config_["swap-icon-label"].isBool()) {
    swap_icon_label = config_["swap-icon-label"].asBool();
  } else {
    spdlog::warn("'swap-icon-label' must be a bool, found '{}'. Using default value (false).",
                 config_["swap-icon-label"].asString());
  }

  if ((rot == 0 || rot == 3) ^ swap_icon_label) {
    box_.add(image_);
    box_.add(label_);
  } else {
    box_.add(label_);
    box_.add(image_);
  }

  event_box_.add(box_);
}

std::tuple<std::string, std::string> AIconLabel::extractIcon(const std::string& input) {
  // A directive is "\0icon\1f<name>\n" (literal backslashes) and does not span lines. The label may
  // hold foreign text, so this is one forward pass: a regex would rescan after every bare prefix.
  static constexpr std::string_view prefix = R"(\0icon\1f)";
  static constexpr std::string_view suffix = R"(\n)";
  if (input.find(prefix) == std::string::npos) {
    return {std::string(), input};
  }
  std::string icon_result;
  std::string label_result;
  label_result.reserve(input.size());

  std::string::size_type pos = 0;
  while (pos < input.size()) {
    const auto eol = std::min(input.find_first_of("\r\n", pos), input.size());
    const auto line = std::string_view(input).substr(0, eol);
    for (auto start = line.find(prefix, pos); start != std::string_view::npos;
         start = line.find(prefix, pos)) {
      const auto name = start + prefix.size();
      // An empty name in the first directive means no icon and an untouched label
      if (icon_result.empty() && line.substr(name, suffix.size()) == suffix) {
        return {std::string(), input};
      }
      const auto end = line.find(suffix, name + 1);
      if (end == std::string_view::npos) {
        break;  // no terminator left on this line, so none for a later prefix either
      }
      if (icon_result.empty()) {
        icon_result = line.substr(name, end - name);
      }
      label_result.append(input, pos, start - pos);
      pos = end + suffix.size();
    }
    label_result.append(input, pos, eol + 1 - pos);
    pos = eol + 1;
  }

  return {std::move(icon_result), std::move(label_result)};
}

auto AIconLabel::update() -> void {
  label_contains_icon = false;

  std::string iconLabel;
  std::string cleanLabel;
  if (embedded_icon_enabled_) {
    std::tie(iconLabel, cleanLabel) = extractIcon(label_.get_label().raw());
  }
  label_contains_icon = !iconLabel.empty();

  if (label_contains_icon) {
    label_.set_markup(cleanLabel);

    if (iconLabel.front() == '/') {
      try {
        // A FIFO or a device would block the main loop in open() or read()
        if (!Glib::file_test(iconLabel, Glib::FILE_TEST_IS_REGULAR)) {
          throw Glib::FileError(Glib::FileError::INVALID_ARGUMENT, "not a regular file");
        }
        int scaled_icon_size = app_icon_size_ * image_.get_scale_factor();
        auto pixbuf = Gdk::Pixbuf::create_from_file(iconLabel, scaled_icon_size, scaled_icon_size);

        auto surface = Gdk::Cairo::create_surface_from_pixbuf(pixbuf, image_.get_scale_factor(),
                                                              image_.get_window());
        image_.set(surface);
        image_.set_visible(true);
      } catch (const Glib::Exception& e) {
        spdlog::warn("Failed to load embedded icon {}: {}", iconLabel, std::string(e.what()));
        image_.set_visible(false);
      }
    } else {
      image_.set_from_icon_name(iconLabel, Gtk::ICON_SIZE_INVALID);
      image_.set_visible(true);
    }
  }

  image_.set_visible(image_.get_visible() && iconEnabled());
  ALabel::update();
}

bool AIconLabel::iconEnabled() const {
  return label_contains_icon || (config_["icon"].isBool() ? config_["icon"].asBool() : false);
}

}  // namespace waybar
