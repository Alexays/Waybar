#include "client.hpp"

#include <gtk4-layer-shell.h>
#include <gtkmm/icontheme.h>
#include <gtkmm/init.h>
#include <gtkmm/settings.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <utility>

#include "ext-idle-notify-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "util/clara.hpp"
#include "util/format.hpp"
#ifdef HAVE_BAR_BLUR
#include "ext-background-effect-v1-client-protocol.h"
#endif

waybar::Client* waybar::Client::inst() {
  static auto* c = new Client();
  return c;
}

void waybar::Client::handleGlobal(void* data, struct wl_registry* registry, uint32_t name,
                                  const char* interface, uint32_t version) {
  auto* client = static_cast<Client*>(data);

  if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0 &&
      version >= ZXDG_OUTPUT_V1_NAME_SINCE_VERSION) {
    if (client->xdg_output_manager != nullptr) {
      zxdg_output_manager_v1_destroy(client->xdg_output_manager);
      client->xdg_output_manager = nullptr;
    }

    client->xdg_output_manager = static_cast<struct zxdg_output_manager_v1*>(wl_registry_bind(
        registry, name, &zxdg_output_manager_v1_interface, ZXDG_OUTPUT_V1_NAME_SINCE_VERSION));

  } else if (strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) == 0) {
    if (client->idle_inhibit_manager != nullptr) {
      zwp_idle_inhibit_manager_v1_destroy(client->idle_inhibit_manager);
      client->idle_inhibit_manager = nullptr;
    }

    client->idle_inhibit_manager = static_cast<struct zwp_idle_inhibit_manager_v1*>(
        wl_registry_bind(registry, name, &zwp_idle_inhibit_manager_v1_interface, 1));
  } else if (strcmp(interface, ext_idle_notifier_v1_interface.name) == 0) {
    // Bind version 2 if available (for get_input_idle_notification), otherwise version 1
    auto bind_version = std::min(version, 2u);
    client->idle_notifier = static_cast<struct ext_idle_notifier_v1 *>(
        wl_registry_bind(registry, name, &ext_idle_notifier_v1_interface, bind_version));
    spdlog::debug("Bound ext-idle-notifier-v1 at version {}", bind_version);
#ifdef HAVE_BAR_BLUR
  } else if (strcmp(interface, ext_background_effect_manager_v1_interface.name) == 0) {
    if (client->background_effect_manager != nullptr) {
      ext_background_effect_manager_v1_destroy(client->background_effect_manager);
      client->background_effect_manager = nullptr;
    }

    client->background_effect_manager = static_cast<struct ext_background_effect_manager_v1*>(
        wl_registry_bind(registry, name, &ext_background_effect_manager_v1_interface, 1));
#endif
  }
}

void waybar::Client::handleGlobalRemove(void* data, struct wl_registry* /*registry*/,
                                        uint32_t name) {
  // Nothing here
}

void waybar::Client::handleOutput(struct waybar_output& output) {
  static const struct zxdg_output_v1_listener xdgOutputListener = {
      .logical_position = [](void*, struct zxdg_output_v1*, int32_t, int32_t) {},
      .logical_size = &handleOutputLogicalSize,
      .done = &handleOutputDone,
      .name = &handleOutputName,
      .description = &handleOutputDescription,
  };
  // owned by output->monitor; no need to destroy
  auto* wl_output = gdk_wayland_monitor_get_wl_output(output.monitor->gobj());
  output.xdg_output.reset(zxdg_output_manager_v1_get_xdg_output(xdg_output_manager, wl_output));
  zxdg_output_v1_add_listener(output.xdg_output.get(), &xdgOutputListener, &output);
}

struct waybar::waybar_output& waybar::Client::getOutput(void* addr) {
  auto it = std::find_if(outputs_.begin(), outputs_.end(),
                         [&addr](const auto& output) { return &output == addr; });
  if (it == outputs_.end()) {
    throw std::runtime_error("Unable to find valid output");
  }
  return *it;
}

std::vector<Json::Value> waybar::Client::getOutputConfigs(struct waybar_output& output) {
  return config.getOutputConfigs(output.name, output.identifier, output.width, output.height);
}

void waybar::Client::handleOutputLogicalSize(void* data, struct zxdg_output_v1* object,
                                             int32_t width, int32_t height) {
  auto client = waybar::Client::inst();
  try {
    auto& output = client->getOutput(data);
    output.width = width;
    output.height = height;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

void waybar::Client::handleOutputDone(void* data, struct zxdg_output_v1* /*xdg_output*/) {
  auto* client = waybar::Client::inst();
  try {
    auto& output = client->getOutput(data);
    /**
     * Multiple .done events may arrive in batch. In this case libwayland would queue
     * xdg_output.destroy and dispatch all pending events, triggering this callback several times
     * for the same output. .done events can also arrive after that for a scale or position changes.
     * We wouldn't want to draw a duplicate bar for each such event either.
     *
     * All the properties we care about are immutable so it's safe to delete the xdg_output object
     * on the first event and use the ptr value to check that the callback was already invoked.
     */
    if (output.xdg_output) {
      output.xdg_output.reset();
      spdlog::debug("Output detection done: {} ({})", output.name, output.identifier);

      client->pending_outputs_.push_back(&output);

      if (!client->bars_scheduled_) {
        client->bars_scheduled_ = true;

        Glib::signal_idle().connect_once([client]() {
          client->createBarsBatch();
        }, Glib::PRIORITY_HIGH_IDLE);
      }
    }
  } catch (const std::exception& e) {
    spdlog::warn("caught exception in zxdg_output_v1_listener::done: {}", e.what());
  }
}

void waybar::Client::createBarsBatch() {
  pending_outputs_.remove_if([this](auto* output) { return std::none_of(outputs_.begin(), outputs_.end(), [&output](const auto& o) { return &o == output; }); });
  for (auto* output : pending_outputs_) {
    try {
      auto configs = getOutputConfigs(*output);
      if (!configs.empty()) {
        for (const auto& config : configs) {
          bars.emplace_back(std::make_unique<Bar>(output, config));
        }
      }
    } catch (const std::exception& e) {
      spdlog::warn("Error creating bar: {}", e.what());
    }
  }

  pending_outputs_.clear();
  bars_scheduled_ = false;
}

void waybar::Client::handleOutputName(void* data, struct zxdg_output_v1* /*xdg_output*/,
                                      const char* name) {
  auto* client = waybar::Client::inst();
  try {
    auto& output = client->getOutput(data);
    output.name = name;
  } catch (const std::exception& e) {
    spdlog::warn("caught exception in zxdg_output_v1_listener::name: {}", e.what());
  }
}

void waybar::Client::handleOutputDescription(void* data, struct zxdg_output_v1* /*xdg_output*/,
                                             const char* description) {
  auto* client = waybar::Client::inst();
  try {
    auto& output = client->getOutput(data);

    // Description format: "identifier (name)"
    auto s = std::string(description);
    auto pos = s.find(" (");
    output.identifier = pos != std::string::npos ? s.substr(0, pos) : s;
  } catch (const std::exception& e) {
    spdlog::warn("caught exception in zxdg_output_v1_listener::description: {}", e.what());
  }
}

void waybar::Client::handleMonitorAdded(Glib::RefPtr<Gdk::Monitor> monitor) {
  auto& output = outputs_.emplace_back();
  // A disconnected output invalidates its monitor before it leaves the display's list
  output.invalidate_conn =
      monitor->signal_invalidate().connect([this, weak = std::weak_ptr<Gdk::Monitor>(monitor)] {
        auto monitor = weak.lock();
        if (!monitor) {
          return;
        }
        spdlog::debug("Output removed: {} {}", monitor->get_manufacturer().c_str(),
                      monitor->get_model().c_str());
        /* This event can be triggered from wl_display_roundtrip called by GTK or our code.
         * Defer destruction of bars for the output to the next iteration of the event loop to avoid
         * deleting objects referenced by currently executed code.
         */
        Glib::signal_idle().connect_once(
            sigc::bind(sigc::mem_fun(*this, &Client::handleDeferredMonitorRemoval), monitor),
            Glib::PRIORITY_HIGH_IDLE);
      });
  output.monitor = std::move(monitor);
  handleOutput(output);
}

void waybar::Client::handleMonitorsChanged(guint position, guint /*removed*/, guint added) {
  auto monitors = gdk_display->get_monitors();
  for (auto i = position; i < position + added; ++i) {
    if (auto monitor = std::dynamic_pointer_cast<Gdk::Monitor>(monitors->get_object(i))) {
      handleMonitorAdded(monitor);
    }
  }
}

void waybar::Client::handleDeferredMonitorRemoval(Glib::RefPtr<Gdk::Monitor> monitor) {
  for (auto it = bars.begin(); it != bars.end();) {
    if ((*it)->output->monitor == monitor) {
      auto output_name = (*it)->output->name;
      (*it)->window.set_visible(false);
      it = bars.erase(it);
      spdlog::info("Bar removed from output: {}", output_name);
    } else {
      ++it;
    }
  }
  outputs_.remove_if([&monitor](const auto& output) { return output.monitor == monitor; });
}

const std::string waybar::Client::getStyle(const std::string& style,
                                           std::optional<Appearance> appearance = std::nullopt) {
  auto gtk_settings = Gtk::Settings::get_default();
  std::optional<std::string> css_file;

  if (style.empty()) {
    std::vector<std::string> search_files;
    switch (appearance.value_or(portal ? portal->getAppearance() : waybar::Appearance::UNKNOWN)) {
      case waybar::Appearance::LIGHT:
        search_files.emplace_back("style-light.css");
        gtk_settings->property_gtk_application_prefer_dark_theme() = false;
        break;
      case waybar::Appearance::DARK:
        search_files.emplace_back("style-dark.css");
        gtk_settings->property_gtk_application_prefer_dark_theme() = true;
        break;
      case waybar::Appearance::UNKNOWN:
        break;
    }
    search_files.emplace_back("style.css");
    css_file = Config::findConfigPath(search_files);
  } else {
    css_file = style;
  }

  if (!css_file) {
    throw std::runtime_error("Missing required resource files");
  }

  spdlog::info("Using CSS file {}", css_file.value());
  return css_file.value();
};

// Returns the rules of `css` written for GTK3 that GTK4 ignores, rewritten for GTK4: GtkPopoverMenu
// nodes for the `menu` and `menuitem` ones, and -gtk-icon-filter for -gtk-icon-effect.
static std::string gtk3CompatCss(const std::string& css) {
  auto is_ident = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_';
  };
  // Rewrites the `menu`/`menuitem` type selectors of one selector, empty when it has none
  auto rewrite = [&](std::string_view selector) {
    std::string out;
    bool found = false;
    for (size_t i = 0; i < selector.size();) {
      if (is_ident(selector[i])) {
        size_t end = i;
        while (end < selector.size() && is_ident(selector[end])) ++end;
        const auto word = selector.substr(i, end - i);
        const char before = i > 0 ? selector[i - 1] : ' ';
        const bool type_selector = before != '.' && before != '#' && before != ':' &&
                                   before != '@' && before != '-' && before != '\\';
        if (type_selector && word == "menu") {
          out += "popover.menu > contents";
          found = true;
        } else if (type_selector && word == "menuitem") {
          out += "modelbutton";
          found = true;
        } else {
          out += word;
        }
        i = end;
      } else {
        out += selector[i++];
      }
    }
    return found ? out : std::string();
  };

  std::string result;
  size_t pos = 0;
  while (pos < css.size()) {
    // Skip comments, then read the prelude of the next rule up to its block
    if (css.compare(pos, 2, "/*") == 0) {
      const auto end = css.find("*/", pos + 2);
      pos = end == std::string::npos ? css.size() : end + 2;
      continue;
    }
    const auto open = css.find_first_of("{;/", pos);
    if (open == std::string::npos) break;
    if (css[open] == '/') {
      if (css.compare(open, 2, "/*") == 0) {
        pos = open;
        const auto end = css.find("*/", pos + 2);
        pos = end == std::string::npos ? css.size() : end + 2;
      } else {
        pos = open + 1;
      }
      continue;
    }
    if (css[open] == ';') {  // an at-rule statement such as @define-color
      pos = open + 1;
      continue;
    }
    const std::string_view prelude(css.data() + pos, open - pos);
    size_t close = open + 1;
    for (int depth = 1; close < css.size() && depth > 0; ++close) {
      if (css[close] == '{') ++depth;
      if (css[close] == '}') --depth;
    }
    std::string selectors;
    if (prelude.find('@') == std::string_view::npos) {
      size_t start = 0;
      int parens = 0;
      for (size_t i = 0; i <= prelude.size(); ++i) {
        if (i < prelude.size() && prelude[i] == '(') ++parens;
        if (i < prelude.size() && prelude[i] == ')') --parens;
        if (i == prelude.size() || (prelude[i] == ',' && parens == 0)) {
          auto rewritten = rewrite(prelude.substr(start, i - start));
          if (!rewritten.empty()) {
            selectors += selectors.empty() ? rewritten : "," + rewritten;
          }
          start = i + 1;
        }
      }
    }
    std::string block = css.substr(open, close - open);
    if (block.find("-gtk-icon-effect") != std::string::npos) {
      static const std::regex effect(R"(-gtk-icon-effect\s*:\s*([a-z]+))");
      static const std::map<std::string, std::string> filters{
          {"dim", "opacity(0.5)"}, {"highlight", "brightness(1.11)"}, {"none", "none"}};
      std::string converted;
      auto last = block.cbegin();
      for (std::sregex_iterator it(block.begin(), block.end(), effect), end; it != end; ++it) {
        converted.append(last, (*it)[0].first);
        auto filter = filters.find((*it)[1].str());
        converted += "-gtk-icon-filter: " + (filter != filters.end() ? filter->second : "none");
        last = (*it)[0].second;
      }
      converted.append(last, block.cend());
      block = std::move(converted);
      if (selectors.empty() && prelude.find('@') == std::string_view::npos) {
        selectors = prelude;
      }
    }
    if (!selectors.empty()) {
      result += selectors;
      result += block;
      result += '\n';
    }
    pos = close;
  }
  return result;
}

auto waybar::Client::setupCss(const std::string& css_file) -> void {
  if (!gdk_display) {
    throw std::runtime_error("No default display");
  }
  std::ifstream file(css_file);
  if (!file.is_open() || !Glib::file_test(css_file, Glib::FileTest::IS_REGULAR)) {
    throw std::runtime_error("Can't open style file");
  }
  std::stringstream content;
  content << file.rdbuf();

  // GTK4 skips what it cannot parse instead of failing the whole file: report it like GTK3 did.
  auto report = [](const Glib::RefPtr<const Gtk::CssSection>& section, const Glib::Error& error) {
    auto start = section->get_start_location();
    spdlog::warn("{}:{}:{}: {}", section->get_file() ? section->get_file()->get_path() : "",
                 start.get_lines() + 1, start.get_line_chars() + 1, error.what());
  };
  auto provider = Gtk::CssProvider::create();
  provider->signal_parsing_error().connect(report);
  provider->load_from_path(css_file);

  for (const auto& old : {css_provider_, compat_css_provider_}) {
    if (old) {
      Gtk::StyleProvider::remove_provider_for_display(gdk_display, old);
    }
  }
  css_provider_ = std::move(provider);
  compat_css_provider_.reset();
  Gtk::StyleProvider::add_provider_for_display(gdk_display, css_provider_,
                                               GTK_STYLE_PROVIDER_PRIORITY_USER);
  if (auto compat_css = gtk3CompatCss(content.str()); !compat_css.empty()) {
    compat_css_provider_ = Gtk::CssProvider::create();
    compat_css_provider_->signal_parsing_error().connect(report);
    compat_css_provider_->load_from_string(compat_css);
    Gtk::StyleProvider::add_provider_for_display(gdk_display, compat_css_provider_,
                                                 GTK_STYLE_PROVIDER_PRIORITY_USER);
  }
#ifdef HAVE_BAR_BLUR
  // The bar's border-radius may have changed without a resize event.
  for (auto& bar : bars) {
    bar->updateBlurRegion();
  }
#endif
}

void waybar::Client::bindInterfaces() {
  registry = wl_display_get_registry(wl_display);
  static const struct wl_registry_listener registry_listener = {
      .global = handleGlobal,
      .global_remove = handleGlobalRemove,
  };
  wl_registry_add_listener(registry, &registry_listener, this);
  wl_display_roundtrip(wl_display);

  if (gtk_layer_is_supported() == 0) {
    throw std::runtime_error("The Wayland compositor does not support wlr-layer-shell protocol");
  }

  if (xdg_output_manager == nullptr) {
    throw std::runtime_error("Failed to acquire required resources.");
  }

  // Disconnect previous signal handlers to prevent duplicate handlers on reload
  monitors_changed_connection_.disconnect();

  // Clear stale outputs from previous run
  outputs_.clear();

  // Also drop any batch state that was left pending from the previous run. On
  // reload the GApplication is swapped but the default main context (and its
  // queued PRIORITY_HIGH_IDLE createBarsBatch source) survives; pending_outputs_
  // would then hold dangling waybar_output* into the just-cleared outputs_ list,
  // which createBarsBatch's address comparison can mis-match if the freed slot is
  // reused. Reset so the next run schedules its batch from a clean state (#4129).
  pending_outputs_.clear();
  bars_scheduled_ = false;

  // add existing outputs and subscribe to updates
  auto monitors = gdk_display->get_monitors();
  handleMonitorsChanged(0, 0, monitors->get_n_items());
  monitors_changed_connection_ = monitors->signal_items_changed().connect(
      sigc::mem_fun(*this, &Client::handleMonitorsChanged));
}

int waybar::Client::main(int argc, char* argv[]) {
  bool show_help = false;
  bool show_version = false;
  std::string config_opt;
  std::string style_opt;
  std::string log_level;
  auto cli = clara::detail::Help(show_help) |
             clara::detail::Opt(show_version)["-v"]["--version"]("Show version") |
             clara::detail::Opt(config_opt, "config")["-c"]["--config"]("Config path") |
             clara::detail::Opt(style_opt, "style")["-s"]["--style"]("Style path") |
             clara::detail::Opt(
                 log_level,
                 "trace|debug|info|warning|error|critical|off")["-l"]["--log-level"]("Log level") |
             clara::detail::Opt(bar_id, "id")["-b"]["--bar"]("Bar id");
  auto res = cli.parse(clara::detail::Args(argc, argv));
  if (!res) {
    spdlog::error("Error in command line: {}", res.errorMessage());
    return 1;
  }
  if (show_help) {
    std::cout << cli << '\n';
    return 0;
  }
  if (show_version) {
    std::cout << "Waybar v" << VERSION << '\n';
    return 0;
  }
  if (!log_level.empty()) {
    spdlog::set_level(spdlog::level::from_str(log_level));
  }
  // No Gtk::Application: it would export Waybar on the session bus, which nothing uses
  if (!main_loop) {
    // Names the application on the accessibility bus, as argv[0] did with GTK3's gtk_init
    g_set_prgname("waybar");
    gtk_init_check();
#if GTK_CHECK_VERSION(4, 16, 0)
    // Text hinted and placed on whole pixels following the gtk-xft settings, as GTK3 draws it
    g_object_set(gtk_settings_get_default(), "gtk-font-rendering", GTK_FONT_RENDERING_MANUAL,
                 nullptr);
#endif
    Gtk::init_gtkmm_internals();
    main_loop = Glib::MainLoop::create();
  }

  gdk_display = Gdk::Display::get_default();
  if (!gdk_display) {
    throw std::runtime_error("Can't find display");
  }

  // Initialize Waybars GTK resources with our custom icons
  Gtk::IconTheme::get_for_display(gdk_display)->add_resource_path("/fr/arouillard/waybar/icons");
  if (!menu_css_provider_) {
    // GTK3 Adwaita menu metrics, above the theme and below the user's style: 6px item padding plus
    // the check indicator room (16px + 7px) that GTK3 menus reserve on both sides.
    menu_css_provider_ = Gtk::CssProvider::create();
    menu_css_provider_->load_from_string(R"(
popover.menu.background > contents { padding: 4px 0; border-radius: 5px; }
popover.menu modelbutton { min-height: 16px; min-width: 40px; padding: 4px 29px; border-radius: 0; }
popover.menu modelbutton:hover, popover.menu modelbutton:selected {
  color: @theme_selected_fg_color; background-color: @theme_selected_bg_color; }
popover.menu modelbutton > image { margin-left: -23px; margin-right: 7px; }
popover.menu check.left, popover.menu radio.left { margin-left: -22px; margin-right: 8px; }
)");
    Gtk::StyleProvider::add_provider_for_display(gdk_display, menu_css_provider_,
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
  if (!GDK_IS_WAYLAND_DISPLAY(gdk_display->gobj())) {
    throw std::runtime_error("Bar need to run under Wayland");
  }
  wl_display = gdk_wayland_display_get_wl_display(gdk_display->gobj());
  config.load(config_opt);
  if (!portal) {
    try {
      portal = std::make_unique<waybar::Portal>();
    } catch (const Glib::Error& e) {
      spdlog::warn(
          "Failed to connect to the desktop portal, light/dark theme detection disabled: {}",
          std::string(e.what()));
    } catch (...) {
      spdlog::warn("Failed to connect to the desktop portal, light/dark theme detection disabled");
    }
  }
  m_cssFile = getStyle(style_opt);
  setupCss(m_cssFile);
  m_cssReloadHelper = std::make_unique<CssReloadHelper>(m_cssFile, [&](const std::string& css_file) { setupCss(css_file); });
  if (portal) {
    portal->signal_appearance_changed().connect([&](waybar::Appearance appearance) {
      auto css_file = getStyle(style_opt, appearance);
      m_cssReloadHelper->changeCssFile(css_file);
      setupCss(css_file);
    });
  }

  auto m_config = config.getConfig();
  if (m_config.isObject() && m_config["reload_style_on_change"].asBool()) {
    m_cssReloadHelper->monitorChanges();
  } else if (m_config.isArray()) {
    for (const auto& conf : m_config) {
      if (conf["reload_style_on_change"].asBool()) {
        m_cssReloadHelper->monitorChanges();
        break;
      }
    }
  }

  bindInterfaces();
  main_loop->run();
  m_cssReloadHelper.reset();  // stop watching css file
  bars.clear();
  return 0;
}

void waybar::Client::reset() {
  main_loop->quit();
  // delete signal handler for css changes
  if (portal) {
    portal->signal_appearance_changed().clear();
  }
}
