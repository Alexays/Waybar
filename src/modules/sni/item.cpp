#include "modules/sni/item.hpp"

#include <fcntl.h>
#include <gdkmm/general.h>
#include <giomm/memoryinputstream.h>
#include <giomm/unixinputstream.h>
#include <glibmm/main.h>
#include <gtkmm/tooltip.h>
#include <spdlog/spdlog.h>
#include <sys/stat.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <typeinfo>
#include <unordered_map>

#include "gdk/gdk.h"
#include "modules/sni/host.hpp"
#include "modules/sni/icon_manager.hpp"
#include "util/format.hpp"  // IWYU pragma: keep
#include "util/gtk_icon.hpp"

template <>
struct fmt::formatter<Glib::VariantBase> : formatter<std::string> {
  bool is_printable(const Glib::VariantBase& value) const {
    auto type = value.get_type_string();
    /* Print only primitive (single character excluding 'v') and short complex types */
    return (type.length() == 1 && islower(type[0]) && type[0] != 'v') || value.get_size() <= 32;
  }

  template <typename FormatContext>
  auto format(const Glib::VariantBase& value, FormatContext& ctx) const {
    if (is_printable(value)) {
      return formatter<std::string>::format(static_cast<std::string>(value.print()), ctx);
    } else {
      return formatter<std::string>::format(value.get_type_string(), ctx);
    }
  }
};

namespace waybar::modules::SNI {

static const Glib::ustring SNI_INTERFACE_NAME = sn_item_interface_info()->name;
static const unsigned UPDATE_DEBOUNCE_TIME = 10;
// Icons are drawn icon-size high times the output scale; pixmaps keep no more than scale 4 needs.
static const int MAX_PIXMAP_SCALE = 4;
// Widest icon an item may show, as a multiple of its height
static const int MAX_ICON_ASPECT_RATIO = 4;
// An icon file named by an item costs no more than an IconPixmap, which D-Bus bounds to 64 MiB
static const goffset MAX_ICON_FILE_SIZE = 64 << 20;
static const guint64 MAX_ICON_FILE_PIXELS = MAX_ICON_FILE_SIZE / 4;

Item::Item(const std::string& bn, const std::string& op, const Json::Value& config, const Bar& bar,
           const std::function<void(Item&)>& on_ready,
           const std::function<void(Item&)>& on_invalidate, const std::function<void()>& on_updated,
           Host& host, const ItemOrderMap& orders)
    : bus_name(bn),
      object_path(op),
      icon_size(16),
      effective_icon_size(0),
      bar_(bar),
      on_ready_(on_ready),
      on_invalidate_(on_invalidate),
      on_updated_(on_updated),
      host_(host),
      orders_(orders) {
  if (config["icon-size"].isUInt()) {
    icon_size = config["icon-size"].asUInt();
  }
  if (config["smooth-scrolling-threshold"].isNumeric()) {
    scroll_threshold_ = config["smooth-scrolling-threshold"].asDouble();
  }
  if (config["show-passive-items"].isBool()) {
    show_passive_ = config["show-passive-items"].asBool();
  }
  if (config["tooltip"].isBool()) {
    show_tooltip_ = config["tooltip"].asBool();
  }

  auto& window = const_cast<Bar&>(bar).window;
  window.signal_configure_event().connect_notify(sigc::mem_fun(*this, &Item::onConfigure));
  event_box.add(image);
  event_box.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
  event_box.signal_button_press_event().connect(sigc::mem_fun(*this, &Item::handleClick));
  event_box.signal_scroll_event().connect(sigc::mem_fun(*this, &Item::handleScroll));
  event_box.signal_enter_notify_event().connect(sigc::mem_fun(*this, &Item::handleMouseEnter));
  event_box.signal_leave_notify_event().connect(sigc::mem_fun(*this, &Item::handleMouseLeave));
  // initial visibility
  event_box.show_all();
  event_box.set_visible(show_passive_);

  cancellable_ = Gio::Cancellable::create();

  auto interface = Glib::wrap(sn_item_interface_info(), true);
  // The bus name is the registering client's choice: never have the bus activate a service for it.
  Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::BUS_TYPE_SESSION, bus_name, object_path,
                                   SNI_INTERFACE_NAME, sigc::mem_fun(*this, &Item::proxyReady),
                                   cancellable_, interface,
                                   Gio::DBus::PROXY_FLAGS_DO_NOT_AUTO_START);
}

Item::~Item() {
  if (this->gtk_menu != nullptr) {
    this->gtk_menu->popdown();
    this->gtk_menu->detach();
  }
  if (this->dbus_menu != nullptr) {
    g_object_weak_unref(G_OBJECT(this->dbus_menu), (GWeakNotify)onMenuDestroyed, this);
    // makeMenu() holds a reference (g_object_ref_sink), and a GtkMenu is a toplevel that GTK keeps
    // in its window list. Without destroying it and dropping the reference, the menu and its whole
    // dbusmenu tree outlive the item, e.g. for every tray item of a removed bar.
    gtk_widget_destroy(GTK_WIDGET(this->dbus_menu));
    g_object_unref(this->dbus_menu);
    this->dbus_menu = nullptr;
    this->gtk_menu = nullptr;
  }
}

bool Item::isReady() const { return ready_; }

bool Item::handleMouseEnter(GdkEventCrossing* const& e) {
  event_box.set_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
  return false;
}

bool Item::handleMouseLeave(GdkEventCrossing* const& e) {
  event_box.unset_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
  return false;
}

void Item::onConfigure(GdkEventConfigure* ev) { this->updateImage(); }

// The proxy caches whatever the item announces, of any name and size, and nothing reads it. A raw
// handler: glibmm's signal_properties_changed() unpacks the names into a collating std::map.
static void dropChangedProperties(GDBusProxy* proxy, GVariant* changed, GStrv /*invalidated*/,
                                  gpointer /*data*/) {
  GVariantIter iter;
  const gchar* name = nullptr;
  g_variant_iter_init(&iter, changed);
  while (g_variant_iter_next(&iter, "{&sv}", &name, nullptr)) {
    g_dbus_proxy_set_cached_property(proxy, name, nullptr);
  }
}

void Item::proxyReady(Glib::RefPtr<Gio::AsyncResult>& result) {
  try {
    this->proxy_ = Gio::DBus::Proxy::create_for_bus_finish(result);
    /* Properties are already cached during object creation */
    auto cached_properties = this->proxy_->get_cached_property_names();
    for (const auto& name : cached_properties) {
      Glib::VariantBase value;
      this->proxy_->get_cached_property(value, name);
      setProperty(name.raw(), value);
    }
    // Updates are fetched with GetAll, so nothing reads the proxy's cache again: don't keep it.
    for (const auto& name : cached_properties) {
      this->proxy_->set_cached_property(name, Glib::VariantBase());
    }
    g_signal_connect(proxy_->gobj(), "g-properties-changed", G_CALLBACK(dropChangedProperties),
                     nullptr);

    this->proxy_->signal_signal().connect(sigc::mem_fun(*this, &Item::onSignal));

    if (this->id.empty() || this->category.empty()) {
      spdlog::error("Invalid Status Notifier Item: {}, {}", bus_name, object_path);
      invalidate();
      return;
    }
    this->updateImage();
    setReady();

  } catch (const Glib::Error& err) {
    spdlog::error("Failed to create DBus Proxy for {} {}: {}", bus_name, object_path, err.what());
    invalidate();
  } catch (const std::exception& err) {
    spdlog::error("Failed to create DBus Proxy for {} {}: {}", bus_name, object_path, err.what());
    invalidate();
  }
}

template <typename T>
T get_variant(const Glib::VariantBase& value) {
  return Glib::VariantBase::cast_dynamic<Glib::Variant<T>>(value).get();
}

template <>
ToolTip get_variant<ToolTip>(const Glib::VariantBase& value) {
  ToolTip result;
  // Unwrap (sa(iiay)ss)
  if (!value || !value.is_of_type(Glib::VariantType("(sa(iiay)ss)"))) {
    throw std::bad_cast();
  }
  auto container = value.cast_dynamic<Glib::VariantContainerBase>(value);
  result.icon_name = get_variant<Glib::ustring>(container.get_child(0));
  result.text = get_variant<Glib::ustring>(container.get_child(2));
  auto description = get_variant<Glib::ustring>(container.get_child(3));
  if (!description.empty()) {
    auto escapedDescription = Glib::Markup::escape_text(description);
    result.text = fmt::format("<b>{}</b>\n{}", result.text, escapedDescription);
  }
  return result;
}

// Names are compared as bytes: Glib::ustring compares with g_utf8_collate(), which normalizes the
// item-supplied name on every comparison and lets look-alike names match.
void Item::setProperty(const std::string& name, Glib::VariantBase& value) {
  try {
    spdlog::trace("Set tray item property: {}.{} = {}", id.empty() ? bus_name : id, name, value);

    if (name == "Category") {
      category = get_variant<std::string>(value);
    } else if (name == "Id") {
      id = get_variant<std::string>(value);

      /*
       * HACK: Electron apps seem to have the same ID, but tooltip seems correct, so use that as ID
       * to pass as the custom icon option. I'm avoiding being disruptive and setting that to the ID
       * itself as I've no idea what this would affect.
       * The tooltip text is converted to lowercase since that's what (most?) themes expect?
       * I still haven't found a way for it to pick from theme automatically, although
       * it might be my theme.
       */
      std::string icon_id = id;
      if (id == "chrome_status_icon_1") {
        Glib::VariantBase value;
        this->proxy_->get_cached_property(value, "ToolTip");
        if (value) {
          tooltip = get_variant<ToolTip>(value);
        }
        if (!tooltip.text.empty()) {
          icon_id = tooltip.text.lowercase();
          setCustomIcon(icon_id);
        }
      } else {
        setCustomIcon(icon_id);
      }

      // Check if this item should be hidden
      if (IconManager::instance().isHidden(icon_id)) {
        spdlog::debug("Hiding tray item with ID: {}", icon_id);
        is_hidden_ = true;
      }
    } else if (name == "Title") {
      title = get_variant<std::string>(value);
      if (tooltip.text.empty()) {
        setTooltipMarkup(title);
      }
    } else if (name == "Status") {
      setStatus(get_variant<Glib::ustring>(value));
    } else if (name == "IconName") {
      if (has_custom_icon_) {
        spdlog::trace("Item '{}': ignoring IconName update, custom icon is set", id);
      } else {
        icon_name = get_variant<std::string>(value);
      }
    } else if (name == "IconPixmap") {
      if (has_custom_icon_) {
        spdlog::trace("Item '{}': ignoring IconPixmap update, custom icon is set", id);
      } else {
        icon_pixmap = this->extractPixBuf(value.gobj());
      }
    } else if (name == "OverlayIconName") {
      overlay_icon_name = get_variant<std::string>(value);
    } else if (name == "OverlayIconPixmap") {
      overlay_icon_pixmap = extractPixBuf(value.gobj());
    } else if (name == "AttentionIconName") {
      attention_icon_name = get_variant<std::string>(value);
    } else if (name == "AttentionIconPixmap") {
      attention_icon_pixmap = extractPixBuf(value.gobj());
    } else if (name == "AttentionMovieName") {
      attention_movie_name = get_variant<std::string>(value);
    } else if (name == "ToolTip") {
      tooltip = get_variant<ToolTip>(value);
      if (!tooltip.text.empty()) {
        setTooltipMarkup(tooltip.text);
      }
    } else if (name == "IconThemePath") {
      icon_theme_path = get_variant<std::string>(value);
    } else if (name == "Menu") {
      menu = get_variant<std::string>(value);
      // Create the menu as soon as the path is known: libdbusmenu fetches the layout
      // asynchronously, and a menu popped up before that completes renders empty.
      makeMenu();
    } else if (name == "ItemIsMenu") {
      item_is_menu = get_variant<bool>(value);
    }
  } catch (const Glib::Error& err) {
    spdlog::warn("Failed to set tray item property: {}.{}, value = {}, err = {}",
                 id.empty() ? bus_name : id, name, value, err.what());
  } catch (const std::exception& err) {
    spdlog::warn("Failed to set tray item property: {}.{}, value = {}, err = {}",
                 id.empty() ? bus_name : id, name, value, err.what());
  }
}

void Item::setStatus(const Glib::ustring& value) {
  status_ = value.lowercase().raw();
  event_box.set_visible(!is_hidden_ && (show_passive_ || status_.compare("passive") != 0));

  auto style = event_box.get_style_context();
  for (const auto& class_name : style->list_classes()) {
    style->remove_class(class_name);
  }
  auto css_class = status_;
  if (css_class.compare("needsattention") == 0) {
    // convert status to dash-case for CSS
    css_class = "needs-attention";
  }
  style->add_class(css_class);
  on_updated_();
}

void Item::setTooltipMarkup(const Glib::ustring& markup) {
  if (show_tooltip_) {
    event_box.set_tooltip_markup(markup);
  }
}

void Item::setReady() {
  if (ready_) {
    return;
  }
  ready_ = true;
  on_ready_(*this);
}

void Item::invalidate() {
  if (ready_) {
    ready_ = false;
  }
  on_invalidate_(*this);
}

void Item::setCustomIcon(const std::string& id) {
  spdlog::debug("SNI tray id: {}", id);

  if (order_ == -1) {
    auto iter = orders_.find(id);
    if (iter != orders_.end()) {
      order_ = iter->second;
      spdlog::debug("reordering tray item {}, order: {}", id, order_);
    } else {
      order_ = 0;
    }
    host_.reorderItems();
  }

  std::string custom_icon = IconManager::instance().getIconForApp(id);
  if (!custom_icon.empty()) {
    if (std::filesystem::exists(custom_icon)) {
      try {
        Glib::RefPtr<Gdk::Pixbuf> custom_pixbuf = Gdk::Pixbuf::create_from_file(custom_icon);
        icon_name = "";  // icon_name has priority over pixmap
        icon_pixmap = custom_pixbuf;
        has_custom_icon_ = true;
      } catch (const Glib::Error& e) {
        spdlog::error("Failed to load custom icon {}: {}", custom_icon, e.what());
      }
    } else {  // if file doesn't exist it's most likely an icon_name
      icon_name = custom_icon;
      has_custom_icon_ = true;
    }
  }
}

void Item::getUpdatedProperties() {
  auto params = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<Glib::ustring>::create(SNI_INTERFACE_NAME)});
  proxy_->call("org.freedesktop.DBus.Properties.GetAll",
               sigc::mem_fun(*this, &Item::processUpdatedProperties), params);
};

void Item::processUpdatedProperties(Glib::RefPtr<Gio::AsyncResult>& _result) {
  try {
    auto result = proxy_->call_finish(_result);
    // extract "a{sv}" from VariantContainerBase
    Glib::Variant<std::map<std::string, Glib::VariantBase>> properties_variant;
    result.get_child(properties_variant);
    auto properties = properties_variant.get();

    for (const auto& [name, value] : properties) {
      if (update_pending_.count(name)) {
        setProperty(name, const_cast<Glib::VariantBase&>(value));
      }
    }

    this->updateImage();
  } catch (const Glib::Error& err) {
    spdlog::warn("Failed to update properties: {}", err.what());
  } catch (const std::exception& err) {
    spdlog::warn("Failed to update properties: {}", err.what());
  }
  update_pending_.clear();
}

/**
 * Mapping from a signal name to a set of possibly changed properties.
 * Commented signals are not handled by the tray module at the moment.
 */
static const std::map<std::string_view, std::set<std::string_view>> signal2props = {
    {"NewTitle", {"Title"}},
    {"NewIcon", {"IconName", "IconPixmap"}},
    {"NewAttentionIcon", {"AttentionIconName", "AttentionIconPixmap", "AttentionMovieName"}},
    {"NewOverlayIcon", {"OverlayIconName", "OverlayIconPixmap"}},
    {"NewIconThemePath", {"IconThemePath"}},
    {"NewToolTip", {"ToolTip"}},
    {"NewStatus", {"Status"}},
    // {"XAyatanaNewLabel", {"XAyatanaLabel"}},
};

void Item::onSignal(const Glib::ustring& sender_name, const Glib::ustring& signal_name,
                    const Glib::VariantContainerBase& arguments) {
  spdlog::trace("Tray item '{}' got signal {}", id, signal_name);
  auto changed = signal2props.find(signal_name.raw());
  if (changed != signal2props.end()) {
    if (update_pending_.empty()) {
      /* Debounce signals and schedule update of all properties.
       * Based on behavior of Plasma dataengine for StatusNotifierItem.
       */
      Glib::signal_timeout().connect_once(sigc::mem_fun(*this, &Item::getUpdatedProperties),
                                          UPDATE_DEBOUNCE_TIME);
    }
    update_pending_.insert(changed->second.begin(), changed->second.end());
  }
}

static void pixbuf_data_deleter(const guint8* data) { g_free((void*)data); }

Glib::RefPtr<Gdk::Pixbuf> Item::extractPixBuf(GVariant* variant) {
  // g_variant_get() leaves the iterator uninitialized on a type mismatch
  if (variant == nullptr || !g_variant_is_of_type(variant, G_VARIANT_TYPE("a(iiay)"))) {
    throw std::bad_cast();
  }
  GVariantIter* it = nullptr;
  g_variant_get(variant, "a(iiay)", &it);
  if (it == nullptr) {
    return Glib::RefPtr<Gdk::Pixbuf>{};
  }
  GVariant* val;
  gint lwidth = 0;
  gint lheight = 0;
  gint width;
  gint height;
  guchar* array = nullptr;
  while (g_variant_iter_loop(it, "(ii@ay)", &width, &height, &val)) {
    if (width > 0 && height > 0 && val != nullptr && width * height > lwidth * lheight) {
      auto size = g_variant_get_size(val);
      /* Sanity check */
      if (size == 4U * width * height) {
        /* Find the largest image */
        gconstpointer data = g_variant_get_data(val);
        if (data != nullptr) {
          if (array != nullptr) {
            g_free(array);
          }
          // We must allocate our own array because the data from GVariant is read-only
          // and we need to modify it to convert ARGB to RGBA.
          array = static_cast<guchar*>(g_malloc(size));

          // Copy and convert ARGB to RGBA in one pass to avoid g_memdup2 overhead
          const guchar* src = static_cast<const guchar*>(data);
          for (gsize i = 0; i < size; i += 4) {
            guchar alpha = src[i];
            array[i] = src[i + 1];
            array[i + 1] = src[i + 2];
            array[i + 2] = src[i + 3];
            array[i + 3] = alpha;
          }

          lwidth = width;
          lheight = height;
        }
      }
    }
  }
  g_variant_iter_free(it);
  if (array != nullptr) {
    auto pixbuf = Gdk::Pixbuf::create_from_data(array, Gdk::Colorspace::COLORSPACE_RGB, true, 8,
                                                lwidth, lheight, 4 * lwidth, &pixbuf_data_deleter);
    // The item picks the size (up to 64 MiB) and the pixmap lives as long as the item does
    const int max_height = std::max(1, icon_size * MAX_PIXMAP_SCALE);
    const int max_width = max_height * MAX_ICON_ASPECT_RATIO;
    if (lheight > max_height || lwidth > max_width) {
      const double factor = std::min(static_cast<double>(max_height) / lheight,
                                     static_cast<double>(max_width) / lwidth);
      pixbuf = pixbuf->scale_simple(std::max(1, static_cast<int>(lwidth * factor)),
                                    std::max(1, static_cast<int>(lheight * factor)),
                                    Gdk::InterpType::INTERP_BILINEAR);
    }
    return pixbuf;
  }
  return Glib::RefPtr<Gdk::Pixbuf>{};
}

void Item::updateImage() {
  auto pixbuf = getIconPixbuf();
  if (!pixbuf) return;
  auto scaled_icon_size = getScaledIconSize();

  // If the loaded icon is not square, assume that the icon height should match the
  // requested icon size, but the width is allowed to be different. As such, if the
  // height of the image does not match the requested icon size, resize the icon such that
  // the aspect ratio is maintained, but the height matches the requested icon size.
  // The width is bounded too: an arbitrarily wide icon would grow the bar past the output.
  const double max_width = std::max(1.0, scaled_icon_size * MAX_ICON_ASPECT_RATIO);
  if (pixbuf->get_height() > 0 &&
      (pixbuf->get_height() != scaled_icon_size || pixbuf->get_width() > max_width)) {
    double width = scaled_icon_size * pixbuf->get_width() / pixbuf->get_height();
    pixbuf = pixbuf->scale_simple(static_cast<int>(std::clamp(width, 1.0, max_width)),
                                  scaled_icon_size, Gdk::InterpType::INTERP_BILINEAR);
    if (!pixbuf) return;
  }

  pixbuf = overlayPixbufs(pixbuf, getOverlayIconPixbuf());

  auto surface =
      Gdk::Cairo::create_surface_from_pixbuf(pixbuf, image.get_scale_factor(), image.get_window());
  image.set(surface);
}

Glib::RefPtr<Gdk::Pixbuf> Item::getIconPixbuf() {
  if (status_ == "needsattention") {
    if (auto attention_pixbuf = getAttentionIconPixbuf()) {
      return attention_pixbuf;
    }
  }

  if (auto pixbuf = loadIconFromNameOrFile(icon_name, true)) {
    return pixbuf;
  }

  if (icon_pixmap) {
    return icon_pixmap;
  }

  if (icon_name.empty()) {
    spdlog::error("Item '{}': No icon name or pixmap given.", id);
  } else {
    spdlog::error("Item '{}': Could not find an icon named '{}' and no pixmap given.", id,
                  icon_name);
  }

  return getIconByName("image-missing", getScaledIconSize());
}

Glib::RefPtr<Gdk::Pixbuf> Item::getAttentionIconPixbuf() {
  if (auto pixbuf = loadIconFromNameOrFile(attention_icon_name, false)) {
    return pixbuf;
  }
  if (auto pixbuf = loadIconFromNameOrFile(attention_movie_name, false)) {
    return pixbuf;
  }
  return attention_icon_pixmap;
}

Glib::RefPtr<Gdk::Pixbuf> Item::getOverlayIconPixbuf() {
  if (auto pixbuf = loadIconFromNameOrFile(overlay_icon_name, false)) {
    return pixbuf;
  }
  return overlay_icon_pixmap;
}

// Loads an icon file an item named, at the given height, or returns null if it is refused. Only a
// regular file is read, through the opened descriptor so it cannot be swapped for a FIFO, and only
// PNG (size checked in its header) and SVG (rendered at that height) are decoded: gdk-pixbuf
// decodes other formats at the size the file declares, whatever height is asked for.
static Glib::RefPtr<Gdk::Pixbuf> loadIconFromFile(const std::string& path, int height) {
  int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
  if (fd == -1) {
    return {};
  }
  auto file_stream = Gio::UnixInputStream::create(fd, true);
  struct stat st;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
      st.st_size > MAX_ICON_FILE_SIZE) {
    spdlog::warn("Tray: refusing icon file '{}': not a regular file of at most {} bytes", path,
                 MAX_ICON_FILE_SIZE);
    return {};
  }
  auto stream = Gio::MemoryInputStream::create();
  auto* data = static_cast<guint8*>(g_malloc(st.st_size));
  g_memory_input_stream_add_data(stream->gobj(), data, st.st_size, g_free);
  gsize size = 0;
  file_stream->read_all(data, st.st_size, size);
  if (size != static_cast<gsize>(st.st_size)) {
    return {};
  }

  static const guint8 png_header[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n',
                                      0,    0,   0,   13,  'I',  'H',  'D',  'R'};
  bool accepted = false;
  if (size >= sizeof(png_header) + 8 && std::memcmp(data, png_header, sizeof(png_header)) == 0) {
    guint32 width = 0;
    guint32 png_height = 0;
    std::memcpy(&width, data + sizeof(png_header), 4);
    std::memcpy(&png_height, data + sizeof(png_header) + 4, 4);
    accepted = static_cast<guint64>(GUINT32_FROM_BE(width)) * GUINT32_FROM_BE(png_height) <=
               MAX_ICON_FILE_PIXELS;
  } else {
    // By content like gdk-pixbuf does; the file name is the item's choice
    g_autofree gchar* type = g_content_type_guess(nullptr, data, size, nullptr);
    accepted = g_content_type_is_a(type, "image/svg+xml") != FALSE;
  }
  if (!accepted) {
    spdlog::warn("Tray: refusing icon file '{}': not a PNG of at most {} pixels or an SVG", path,
                 MAX_ICON_FILE_PIXELS);
    return {};
  }
  return Gdk::Pixbuf::create_from_stream_at_scale(stream, -1, height, true);
}

Glib::RefPtr<Gdk::Pixbuf> Item::loadIconFromNameOrFile(const std::string& name, bool log_failure) {
  if (name.empty()) {
    return {};
  }

  try {
    if (name.front() == '/') {
      if (auto pixbuf = loadIconFromFile(name, getScaledIconSize())) {
        return pixbuf;
      }
    }
  } catch (const Glib::Error& e) {
    if (log_failure) {
      spdlog::warn("Item '{}': {}", id, static_cast<std::string>(e.what()));
    }
  }

  try {
    return getIconByName(name, getScaledIconSize());
  } catch (const Glib::Error& e) {
    if (log_failure) {
      spdlog::trace("Item '{}': {}", id, static_cast<std::string>(e.what()));
    }
  }

  return {};
}

Glib::RefPtr<Gdk::Pixbuf> Item::overlayPixbufs(const Glib::RefPtr<Gdk::Pixbuf>& base,
                                               const Glib::RefPtr<Gdk::Pixbuf>& overlay) {
  if (!base || !overlay) {
    return base;
  }

  auto composed = base->copy();
  if (!composed) {
    return base;
  }

  int overlay_target_size =
      std::max(1, std::min(composed->get_width(), composed->get_height()) / 2);
  auto scaled_overlay = overlay->scale_simple(overlay_target_size, overlay_target_size,
                                              Gdk::InterpType::INTERP_BILINEAR);
  if (!scaled_overlay) {
    return composed;
  }

  int dest_x = std::max(0, composed->get_width() - scaled_overlay->get_width());
  int dest_y = std::max(0, composed->get_height() - scaled_overlay->get_height());
  scaled_overlay->composite(composed, dest_x, dest_y, scaled_overlay->get_width(),
                            scaled_overlay->get_height(), dest_x, dest_y, 1.0, 1.0,
                            Gdk::InterpType::INTERP_BILINEAR, 255);
  return composed;
}

// Finds an icon in the item's IconThemePath, where items keep their icons: in
// <path>/hicolor/<size>/<context>/ (closest size first), then in <path>/. The directory must not
// become a GtkIconTheme search path: GTK maps any icon-theme.cache there without validating it.
static std::string findIconInThemePath(const std::string& theme_path, const std::string& name,
                                       int request_size) {
  namespace fs = std::filesystem;

  if (name.find('/') != std::string::npos) {
    return {};
  }

  std::error_code ec;
  auto find_in_dir = [&name, &ec](const fs::path& dir) -> std::string {
    // Same order of preference as GTK
    for (const auto* suffix : {".symbolic.png", ".png", ".svg"}) {
      auto file = dir / (name + suffix);
      if (fs::is_regular_file(file, ec)) {
        return file.string();
      }
    }
    return {};
  };

  std::string best;
  int best_difference = 0;
  const fs::directory_iterator end;
  for (fs::directory_iterator size_dir(fs::path(theme_path) / "hicolor", ec); size_dir != end;
       size_dir.increment(ec)) {
    // "16x16" or "16x16@2x"; anything else, e.g. "scalable", fits every size
    int size = 0;
    int scale = 1;
    int difference = 0;
    if (std::sscanf(size_dir->path().filename().c_str(), "%4dx%*4d@%1d", &size, &scale) > 0) {
      difference = std::abs((size * scale) - request_size);
    }
    if (!best.empty() && difference >= best_difference) {
      continue;
    }
    for (fs::directory_iterator context_dir(size_dir->path(), ec); context_dir != end;
         context_dir.increment(ec)) {
      if (auto file = find_in_dir(context_dir->path()); !file.empty()) {
        best = file;
        best_difference = difference;
        break;
      }
    }
  }
  return !best.empty() ? best : find_in_dir(theme_path);
}

Glib::RefPtr<Gdk::Pixbuf> Item::getIconByName(const std::string& name, int request_size) {
  if (!icon_theme_path.empty()) {
    auto file = findIconInThemePath(icon_theme_path, name, request_size);
    auto pixbuf = file.empty() ? Glib::RefPtr<Gdk::Pixbuf>() : loadIconFromFile(file, request_size);
    if (pixbuf) {
      // GTK draws symbolic icons in the foreground color; its other symbolic colors are not kept
      if ((file.ends_with("-symbolic.svg") || file.ends_with(".symbolic.png")) &&
          pixbuf->get_has_alpha() && pixbuf->get_n_channels() == 4) {
        auto style = event_box.get_style_context();
        auto color = style->get_color(style->get_state());
        const auto red = static_cast<guint8>(color.get_red() * 255);
        const auto green = static_cast<guint8>(color.get_green() * 255);
        const auto blue = static_cast<guint8>(color.get_blue() * 255);
        const double alpha = color.get_alpha();
        for (int y = 0; y < pixbuf->get_height(); ++y) {
          guint8* pixel = pixbuf->get_pixels() + (y * pixbuf->get_rowstride());
          for (int x = 0; x < pixbuf->get_width(); ++x, pixel += 4) {
            pixel[0] = red;
            pixel[1] = green;
            pixel[2] = blue;
            pixel[3] = static_cast<guint8>(pixel[3] * alpha);
          }
        }
      }
      return pixbuf;
    }
  }
  return DefaultGtkIconThemeWrapper::load_icon(name.c_str(), request_size,
                                               Gtk::IconLookupFlags::ICON_LOOKUP_FORCE_SIZE,
                                               event_box.get_style_context());
}

double Item::getScaledIconSize() {
  // apply the scale factor from the Gtk window to the requested icon size
  return icon_size * image.get_scale_factor();
}

void Item::onMenuDestroyed(Item* self, GObject* old_menu_pointer) {
  if (old_menu_pointer == reinterpret_cast<GObject*>(self->dbus_menu)) {
    self->gtk_menu = nullptr;
    self->dbus_menu = nullptr;
  }
}

void Item::makeMenu() {
  if (gtk_menu == nullptr && !menu.empty()) {
    dbus_menu = dbusmenu_gtkmenu_new(bus_name.data(), menu.data());
    if (dbus_menu != nullptr) {
      g_object_ref_sink(G_OBJECT(dbus_menu));
      g_object_weak_ref(G_OBJECT(dbus_menu), (GWeakNotify)onMenuDestroyed, this);
      // Provide an accel group to the dbusmenu client. Without one, items that export menu
      // accelerators (e.g. Mattermost) trigger gtk_widget_set_accel_path() with a NULL accel group,
      // which raises a Gtk-CRITICAL and corrupts menu state (or aborts under fatal-criticals).
      DbusmenuGtkClient* client = dbusmenu_gtkmenu_get_client(DBUSMENU_GTKMENU(dbus_menu));
      if (client != nullptr) {
        GtkAccelGroup* accel_group = gtk_accel_group_new();
        dbusmenu_gtkclient_set_accel_group(client, accel_group);
        g_object_unref(accel_group);
        // The client adds the menu's IconThemePath to the default icon theme of every module; it
        // belongs to the item, like the item's own (see findIconInThemePath).
        g_signal_handlers_disconnect_matched(
            client, G_SIGNAL_MATCH_ID,
            g_signal_lookup(DBUSMENU_CLIENT_SIGNAL_ICON_THEME_DIRS_CHANGED, DBUSMENU_TYPE_CLIENT),
            0, nullptr, nullptr, nullptr);
      }
      gtk_menu = Glib::wrap(GTK_MENU(dbus_menu));
      gtk_menu->attach_to_widget(event_box);
    }
  }
  // Manually reset prelight to make sure the tray item doesn't stay in a hover state even though
  // the menu is focused
  event_box.unset_state_flags(Gtk::StateFlags::STATE_FLAG_PRELIGHT);
}

bool Item::hasMenuLayout() const {
  if (dbus_menu == nullptr) {
    return false;
  }
  DbusmenuGtkClient* client = dbusmenu_gtkmenu_get_client(dbus_menu);
  return client != nullptr && dbusmenu_client_get_root(DBUSMENU_CLIENT(client)) != nullptr;
}

bool Item::handleClick(GdkEventButton* const& ev) {
  if (!proxy_) {
    return false;
  }
  auto parameters = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<int>::create(ev->x_root + bar_.x_global),
       Glib::Variant<int>::create(ev->y_root + bar_.y_global)});
  if ((ev->button == 1 && item_is_menu) || ev->button == 3) {
    makeMenu();
    // Some items advertise a Menu path without serving com.canonical.dbusmenu (e.g. Steam), so
    // the client never gets a layout root. Use the item's own ContextMenu for those instead of
    // popping up an empty GTK menu.
    if (gtk_menu != nullptr && hasMenuLayout()) {
#if GTK_CHECK_VERSION(3, 22, 0)
      gtk_menu->popup_at_pointer(reinterpret_cast<GdkEvent*>(ev));
#else
      gtk_menu->popup(ev->button, ev->time);
#endif
      return true;
    } else {
      proxy_->call("ContextMenu", parameters);
      return true;
    }
  } else if (ev->button == 1) {
    proxy_->call("Activate", parameters);
    return true;
  } else if (ev->button == 2) {
    proxy_->call("SecondaryActivate", parameters);
    return true;
  }
  return false;
}

bool Item::handleScroll(GdkEventScroll* const& ev) {
  if (!proxy_) {
    return false;
  }
  int dx = 0, dy = 0;
  switch (ev->direction) {
    case GDK_SCROLL_UP:
      dy = -1;
      break;
    case GDK_SCROLL_DOWN:
      dy = 1;
      break;
    case GDK_SCROLL_LEFT:
      dx = -1;
      break;
    case GDK_SCROLL_RIGHT:
      dx = 1;
      break;
    case GDK_SCROLL_SMOOTH:
      distance_scrolled_x_ += ev->delta_x;
      distance_scrolled_y_ += ev->delta_y;
      // check against the configured threshold and ensure that the absolute value >= 1
      if (distance_scrolled_x_ > scroll_threshold_) {
        dx = (int)lround(std::max(distance_scrolled_x_, 1.0));
        distance_scrolled_x_ = 0;
      } else if (distance_scrolled_x_ < -scroll_threshold_) {
        dx = (int)lround(std::min(distance_scrolled_x_, -1.0));
        distance_scrolled_x_ = 0;
      }
      if (distance_scrolled_y_ > scroll_threshold_) {
        dy = (int)lround(std::max(distance_scrolled_y_, 1.0));
        distance_scrolled_y_ = 0;
      } else if (distance_scrolled_y_ < -scroll_threshold_) {
        dy = (int)lround(std::min(distance_scrolled_y_, -1.0));
        distance_scrolled_y_ = 0;
      }
      break;
  }
  if (dx != 0) {
    auto parameters = Glib::VariantContainerBase::create_tuple(
        {Glib::Variant<int>::create(dx), Glib::Variant<Glib::ustring>::create("horizontal")});
    proxy_->call("Scroll", parameters);
  }
  if (dy != 0) {
    auto parameters = Glib::VariantContainerBase::create_tuple(
        {Glib::Variant<int>::create(dy), Glib::Variant<Glib::ustring>::create("vertical")});
    proxy_->call("Scroll", parameters);
  }
  return true;
}

}  // namespace waybar::modules::SNI
