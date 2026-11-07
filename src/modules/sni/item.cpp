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
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string_view>
#include <typeinfo>
#include <unordered_map>

#include "gdk/gdk.h"
#include "modules/sni/host.hpp"
#include "modules/sni/icon_manager.hpp"
#include "util/css_class.hpp"
#include "util/format.hpp"  // IWYU pragma: keep
#include "util/gtk_icon.hpp"
#include "util/menu_a11y.hpp"
#include "util/tooltip.hpp"

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
// Largest dbusmenu layout accepted from an item, see filterMenuLayout()
static const gsize MENU_MAX_ENTRIES = 1000;
// Largest PNG accepted as the icon of a menu entry, which GTK draws at 16 pixels
static const guint64 MENU_ICON_MAX_PIXELS = 512 * 512;

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

  // Redraws the pixmap at the device scale of the output the bar is on
  image.property_scale_factor().signal_changed().connect(sigc::mem_fun(*this, &Item::updateImage));
  event_box.set_child(image);
  click_ = Gtk::GestureClick::create();
  click_->set_button(0);
  click_->signal_pressed().connect(sigc::mem_fun(*this, &Item::handleClick));
  event_box.add_controller(click_);
  scroll_ = Gtk::EventControllerScroll::create();
  scroll_->set_flags(Gtk::EventControllerScroll::Flags::BOTH_AXES);
  scroll_->signal_scroll().connect(sigc::mem_fun(*this, &Item::handleScroll), false);
  event_box.add_controller(scroll_);
  // initial visibility
  event_box.set_visible(show_passive_);

  cancellable_ = Gio::Cancellable::create();

  auto interface = Glib::wrap(sn_item_interface_info(), true);
  // The bus name is the registering client's choice: never have the bus activate a service for it.
  Gio::DBus::Proxy::create_for_bus(Gio::DBus::BusType::SESSION, bus_name, object_path,
                                   SNI_INTERFACE_NAME, sigc::mem_fun(*this, &Item::proxyReady),
                                   cancellable_, interface,
                                   Gio::DBus::ProxyFlags::DO_NOT_AUTO_START);
}

Item::~Item() {
  menu_rebuild_.disconnect();
  if (gtk_menu != nullptr) {
    gtk_menu->unparent();
    delete gtk_menu;
    gtk_menu = nullptr;
  }
  if (dbus_menu != nullptr) {
    // A pending GetLayout keeps the client alive past the unref below: its reply must not run the
    // handlers of this item, on the client or on any of its menu items.
    g_signal_handlers_disconnect_by_data(dbus_menu, this);
    if (DbusmenuMenuitem* root = dbusmenu_client_get_root(dbus_menu); root != nullptr) {
      dbusmenu_menuitem_foreach(
          root,
          [](DbusmenuMenuitem* item, gpointer self) {
            g_signal_handlers_disconnect_by_data(item, self);
          },
          this);
    }
    g_object_unref(dbus_menu);
    dbus_menu = nullptr;
  }
}

bool Item::isReady() const { return ready_; }

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
  result.title = get_variant<Glib::ustring>(container.get_child(2));
  result.text = Glib::Markup::escape_text(result.title);
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
        if (!tooltip.title.empty()) {
          icon_id = tooltip.title.lowercase();
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
        setTooltipMarkup(Glib::Markup::escape_text(title));
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

  event_box.set_css_classes({});
  auto css_class = status_;
  if (css_class.compare("needsattention") == 0) {
    // convert status to dash-case for CSS
    css_class = "needs-attention";
  }
  waybar::util::try_add_class(event_box, css_class);
  on_updated_();
}

void Item::setTooltipMarkup(const Glib::ustring& markup) {
  if (show_tooltip_) {
    waybar::util::set_tooltip_markup(event_box, markup);
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
    if (width > 0 && height > 0 && val != nullptr &&
        static_cast<guint64>(width) * height > static_cast<guint64>(lwidth) * lheight) {
      auto size = g_variant_get_size(val);
      /* Sanity check, in 64 bits so that huge dimensions cannot wrap around */
      if (size == 4 * static_cast<guint64>(width) * height) {
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
    auto pixbuf = Gdk::Pixbuf::create_from_data(array, Gdk::Colorspace::RGB, true, 8, lwidth,
                                                lheight, 4 * lwidth, &pixbuf_data_deleter);
    // The item picks the size (up to 64 MiB) and the pixmap lives as long as the item does
    const int max_height = std::max(1, icon_size * MAX_PIXMAP_SCALE);
    const int max_width = max_height * MAX_ICON_ASPECT_RATIO;
    if (lheight > max_height || lwidth > max_width) {
      const double factor = std::min(static_cast<double>(max_height) / lheight,
                                     static_cast<double>(max_width) / lwidth);
      pixbuf = pixbuf->scale_simple(std::max(1, static_cast<int>(lwidth * factor)),
                                    std::max(1, static_cast<int>(lheight * factor)),
                                    Gdk::InterpType::BILINEAR);
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
                                  scaled_icon_size, Gdk::InterpType::BILINEAR);
    if (!pixbuf) return;
  }

  pixbuf = overlayPixbufs(pixbuf, getOverlayIconPixbuf());

  image.set_pixbuf(pixbuf, image.get_scale_factor());
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

// Whether data is a PNG of at most max_pixels pixels, as its header says
static bool isAcceptedPng(const guint8* data, gsize size, guint64 max_pixels) {
  static const guint8 png_header[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n',
                                      0,    0,   0,   13,  'I',  'H',  'D',  'R'};
  if (size < sizeof(png_header) + 8 || std::memcmp(data, png_header, sizeof(png_header)) != 0) {
    return false;
  }
  guint32 width = 0;
  guint32 height = 0;
  std::memcpy(&width, data + sizeof(png_header), 4);
  std::memcpy(&height, data + sizeof(png_header) + 4, 4);
  return static_cast<guint64>(GUINT32_FROM_BE(width)) * GUINT32_FROM_BE(height) <= max_pixels;
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

  bool accepted = isAcceptedPng(data, size, MAX_ICON_FILE_PIXELS);
  if (!accepted) {
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
  auto scaled_overlay =
      overlay->scale_simple(overlay_target_size, overlay_target_size, Gdk::InterpType::BILINEAR);
  if (!scaled_overlay) {
    return composed;
  }

  int dest_x = std::max(0, composed->get_width() - scaled_overlay->get_width());
  int dest_y = std::max(0, composed->get_height() - scaled_overlay->get_height());
  scaled_overlay->composite(composed, dest_x, dest_y, scaled_overlay->get_width(),
                            scaled_overlay->get_height(), dest_x, dest_y, 1.0, 1.0,
                            Gdk::InterpType::BILINEAR, 255);
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
  Glib::RefPtr<Gdk::Pixbuf> pixbuf;
  bool symbolic = false;
  if (!icon_theme_path.empty()) {
    auto file = findIconInThemePath(icon_theme_path, name, request_size);
    if (!file.empty()) {
      pixbuf = loadIconFromFile(file, request_size);
      symbolic = file.ends_with("-symbolic.svg") || file.ends_with(".symbolic.png");
    }
  }
  if (!pixbuf) {
    pixbuf = DefaultGtkIconThemeWrapper::load_icon(name, request_size);
    symbolic = name.ends_with("-symbolic");
  }
  // GTK3 carried its own copies of the stock icons (dialog-information, media-playback-start...),
  // which themes now often ship only as symbolic icons
  if (!pixbuf && !symbolic) {
    pixbuf = DefaultGtkIconThemeWrapper::load_icon(name + "-symbolic", request_size);
    symbolic = true;
  }
  // GTK draws symbolic icons in the foreground color; its other symbolic colors are not kept
  if (pixbuf && symbolic && pixbuf->get_has_alpha() && pixbuf->get_n_channels() == 4) {
    const auto color = event_box.get_color();
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

double Item::getScaledIconSize() {
  // apply the scale factor from the Gtk window to the requested icon size
  return icon_size * image.get_scale_factor();
}

// Counts the entries below a "(ia{sv}av)" layout node, giving up once there are too many
static gsize countMenuEntries(GVariant* node, gsize count = 0) {
  g_autoptr(GVariant) children = g_variant_get_child_value(node, 2);
  const gsize n_children = g_variant_n_children(children);
  count += n_children;
  for (gsize i = 0; i < n_children && count <= MENU_MAX_ENTRIES; ++i) {
    g_autoptr(GVariant) boxed = g_variant_get_child_value(children, i);
    g_autoptr(GVariant) child = g_variant_get_variant(boxed);
    if (!g_variant_is_of_type(child, G_VARIANT_TYPE("(ia{sv}av)"))) {
      return MENU_MAX_ENTRIES + 1;
    }
    count = countMenuEntries(child, count);
  }
  return count;
}

// libdbusmenu fetches an item's whole layout itself and builds it on the main loop in time
// quadratic to its size, with no hook before that. So oversized GetLayout replies are turned into
// an error before dispatch, on the GDBus worker thread.
static GDBusMessage* filterMenuLayout(GDBusConnection* /*connection*/, GDBusMessage* message,
                                      gboolean incoming, gpointer /*data*/) {
  if (!incoming || g_dbus_message_get_message_type(message) != G_DBUS_MESSAGE_TYPE_METHOD_RETURN ||
      g_strcmp0(g_dbus_message_get_signature(message), "u(ia{sv}av)") != 0) {
    return message;
  }
  g_autoptr(GVariant) root = g_variant_get_child_value(g_dbus_message_get_body(message), 1);
  if (countMenuEntries(root) <= MENU_MAX_ENTRIES) {
    return message;
  }
  const gchar* sender = g_dbus_message_get_sender(message);
  auto reason = fmt::format("Ignoring the tray menu of {}: malformed or more than {} entries",
                            sender != nullptr ? sender : "", MENU_MAX_ENTRIES);
  spdlog::warn("{}", reason);
  // An error reply makes libdbusmenu give up at once instead of waiting for a timeout
  GDBusMessage* error = g_dbus_message_new();
  g_dbus_message_set_message_type(error, G_DBUS_MESSAGE_TYPE_ERROR);
  g_dbus_message_set_reply_serial(error, g_dbus_message_get_reply_serial(message));
  g_dbus_message_set_error_name(error, "org.freedesktop.DBus.Error.LimitsExceeded");
  g_dbus_message_set_body(error, g_variant_new("(s)", reason.c_str()));
  g_object_unref(message);
  return error;
}

// Whether a boolean property is set, like libdbusmenu-gtk: true when unset, false when mistyped
static bool menuItemFlag(DbusmenuMenuitem* item, const gchar* name) {
  GVariant* value = dbusmenu_menuitem_property_get_variant(item, name);
  return value == nullptr ||
         (g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN) && g_variant_get_boolean(value));
}

static bool menuItemToggled(DbusmenuMenuitem* item) {
  GVariant* value =
      dbusmenu_menuitem_property_get_variant(item, DBUSMENU_MENUITEM_PROP_TOGGLE_STATE);
  return value != nullptr && g_variant_is_of_type(value, G_VARIANT_TYPE_INT32) &&
         g_variant_get_int32(value) == DBUSMENU_MENUITEM_TOGGLE_STATE_CHECKED;
}

void Item::makeMenu() {
  if (dbus_menu != nullptr || menu.empty()) {
    return;
  }
  // libdbusmenu uses the shared session bus connection of the proxies: one filter covers all
  [[maybe_unused]] static const guint layout_filter = g_dbus_connection_add_filter(
      proxy_->get_connection()->gobj(), filterMenuLayout, nullptr, nullptr);
  dbus_menu = dbusmenu_client_new(bus_name.c_str(), menu.c_str());
  if (dbus_menu == nullptr) {
    return;
  }
  auto on_layout_updated =
      +[](DbusmenuClient* /*client*/, Item* self) { self->queueMenuRebuild(); };
  auto on_root_changed = +[](DbusmenuClient* /*client*/, DbusmenuMenuitem* /*root*/, Item* self) {
    self->queueMenuRebuild();
  };
  g_signal_connect(dbus_menu, DBUSMENU_CLIENT_SIGNAL_LAYOUT_UPDATED, G_CALLBACK(on_layout_updated),
                   this);
  g_signal_connect(dbus_menu, DBUSMENU_CLIENT_SIGNAL_ROOT_CHANGED, G_CALLBACK(on_root_changed),
                   this);
  g_signal_connect(dbus_menu, DBUSMENU_CLIENT_SIGNAL_NEW_MENUITEM, G_CALLBACK(&Item::onMenuNewItem),
                   this);
}

bool Item::hasMenuLayout() const {
  return dbus_menu != nullptr && dbusmenu_client_get_root(dbus_menu) != nullptr;
}

// The client emits this once an item it added to the tree has all its properties
void Item::onMenuNewItem(DbusmenuClient* /*client*/, DbusmenuMenuitem* item, Item* self) {
  g_signal_connect(item, DBUSMENU_MENUITEM_SIGNAL_PROPERTY_CHANGED,
                   G_CALLBACK(&Item::onMenuPropertyChanged), self);
  self->queueMenuRebuild();
}

void Item::onMenuPropertyChanged(DbusmenuMenuitem* item, const gchar* name, GVariant* /*value*/,
                                 Item* self) {
  const bool enabled = strcmp(name, DBUSMENU_MENUITEM_PROP_ENABLED) == 0;
  if (!self->menu_dirty_ && (enabled || strcmp(name, DBUSMENU_MENUITEM_PROP_TOGGLE_STATE) == 0)) {
    // The shown model stays: GTK follows the state of its actions
    GAction* action =
        g_action_map_lookup_action(G_ACTION_MAP(self->menu_actions_->gobj()),
                                   std::to_string(dbusmenu_menuitem_get_id(item)).c_str());
    if (action == nullptr) {
      return;
    }
    if (enabled) {
      g_simple_action_set_enabled(G_SIMPLE_ACTION(action),
                                  menuItemFlag(item, DBUSMENU_MENUITEM_PROP_ENABLED));
    } else if (const GVariantType* type = g_action_get_state_type(action); type != nullptr) {
      const bool on = menuItemToggled(item);
      g_simple_action_set_state(G_SIMPLE_ACTION(action),
                                g_variant_type_equal(type, G_VARIANT_TYPE_BOOLEAN)
                                    ? g_variant_new_boolean(on)
                                    : g_variant_new_string(on ? "1" : "0"));
    }
    return;
  }
  static constexpr std::array<std::string_view, 8> shown = {
      DBUSMENU_MENUITEM_PROP_LABEL,         DBUSMENU_MENUITEM_PROP_VISIBLE,
      DBUSMENU_MENUITEM_PROP_TYPE,          DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE,
      DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY, DBUSMENU_MENUITEM_PROP_ICON_NAME,
      DBUSMENU_MENUITEM_PROP_ICON_DATA,     DBUSMENU_MENUITEM_PROP_SHORTCUT};
  if (std::ranges::find(shown, name) != shown.end()) {
    self->queueMenuRebuild();
  }
}

// Clients update menus in bursts of signals: the model is rebuilt once they are handled, and only
// while it is shown; a hidden menu is rebuilt when it pops up.
void Item::queueMenuRebuild() {
  menu_dirty_ = true;
  if (gtk_menu != nullptr && gtk_menu->get_visible() && !menu_rebuild_.connected()) {
    menu_rebuild_ = Glib::signal_idle().connect([this] {
      rebuildMenu();
      return false;
    });
  }
}

void Item::rebuildMenu() {
  menu_dirty_ = false;
  GMenu* model = g_menu_new();
  auto actions = Gio::SimpleActionGroup::create();
  if (DbusmenuMenuitem* root = dbusmenu_client_get_root(dbus_menu); root != nullptr) {
    appendMenuItems(model, root, actions->gobj());
  }
  menu_actions_ = std::move(actions);
  gtk_menu->insert_action_group("tray", menu_actions_);
  if (GMenuModel* shown = gtk_popover_menu_get_menu_model(gtk_menu->gobj()); shown != nullptr) {
    syncMenu(G_MENU(shown), G_MENU_MODEL(model));
  } else {
    gtk_popover_menu_set_menu_model(gtk_menu->gobj(), G_MENU_MODEL(model));
  }
  g_object_unref(model);
  if (gtk_menu->get_mapped()) {
    util::update_menu_items(GTK_WIDGET(gtk_menu->gobj()), true);
  }
}

// Updates the shown menu in place, like libdbusmenu-gtk updates its widgets: GTK recreates the
// widgets of replaced items, which would close their open submenus.
void Item::syncMenu(GMenu* shown, GMenuModel* model) {
  const int n_items = g_menu_model_get_n_items(model);
  for (int i = 0; i < n_items; ++i) {
    bool same = i < g_menu_model_get_n_items(G_MENU_MODEL(shown));
    int n_attributes = 0;
    if (same) {
      GMenuAttributeIter* it = g_menu_model_iterate_item_attributes(G_MENU_MODEL(shown), i);
      const gchar* name = nullptr;
      GVariant* value = nullptr;
      while (same && g_menu_attribute_iter_get_next(it, &name, &value)) {
        g_autoptr(GVariant) other = g_menu_model_get_item_attribute_value(model, i, name, nullptr);
        same = other != nullptr && g_variant_equal(value, other);
        g_variant_unref(value);
        ++n_attributes;
      }
      g_object_unref(it);
    }
    if (same) {
      GMenuAttributeIter* it = g_menu_model_iterate_item_attributes(model, i);
      while (g_menu_attribute_iter_next(it)) {
        --n_attributes;
      }
      g_object_unref(it);
      same = n_attributes == 0;
    }
    // Both models come from appendMenuItems: an item has at most one link, a section or a submenu
    GMenuModel* link = nullptr;
    const gchar* link_name = nullptr;
    if (same) {
      GMenuLinkIter* it = g_menu_model_iterate_item_links(model, i);
      if (g_menu_link_iter_get_next(it, &link_name, &link)) {
        g_autoptr(GMenuModel) shown_link =
            g_menu_model_get_item_link(G_MENU_MODEL(shown), i, link_name);
        if (shown_link != nullptr) {
          syncMenu(G_MENU(shown_link), link);
        }
        same = shown_link != nullptr;
        g_object_unref(link);
      } else {
        g_autoptr(GMenuLinkIter) shown_links =
            g_menu_model_iterate_item_links(G_MENU_MODEL(shown), i);
        same = !g_menu_link_iter_next(shown_links);
      }
      g_object_unref(it);
    }
    if (!same) {
      GMenuItem* item = g_menu_item_new_from_model(model, i);
      if (i < g_menu_model_get_n_items(G_MENU_MODEL(shown))) {
        g_menu_remove(shown, i);
      }
      g_menu_insert_item(shown, i, item);
      g_object_unref(item);
    }
  }
  for (int i = g_menu_model_get_n_items(G_MENU_MODEL(shown)); i > n_items; --i) {
    g_menu_remove(shown, i - 1);
  }
}

// Maps the children of a dbusmenu item to GMenu items, like libdbusmenu-gtk maps them to widgets:
// only standard items and separators, which delimit sections, and only "submenu" children.
void Item::appendMenuItems(GMenu* menu, DbusmenuMenuitem* parent, GSimpleActionGroup* actions) {
  GMenu* section = g_menu_new();
  for (GList* child = dbusmenu_menuitem_get_children(parent); child != nullptr;
       child = child->next) {
    auto* item = DBUSMENU_MENUITEM(child->data);
    if (!menuItemFlag(item, DBUSMENU_MENUITEM_PROP_VISIBLE)) {
      continue;
    }
    const gchar* type = dbusmenu_menuitem_property_get(item, DBUSMENU_MENUITEM_PROP_TYPE);
    if (g_strcmp0(type, DBUSMENU_CLIENT_TYPES_SEPARATOR) == 0) {
      if (g_menu_model_get_n_items(G_MENU_MODEL(section)) > 0) {
        g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));
        g_object_unref(section);
        section = g_menu_new();
      }
      continue;
    }
    if (type != nullptr && strcmp(type, DBUSMENU_CLIENT_TYPES_DEFAULT) != 0) {
      continue;
    }

    // Labels use "_" mnemonics in dbusmenu and GMenu alike
    const gchar* label = dbusmenu_menuitem_property_get(item, DBUSMENU_MENUITEM_PROP_LABEL);
    GMenuItem* entry = g_menu_item_new(label, nullptr);
    auto name = std::to_string(dbusmenu_menuitem_get_id(item));
    const gchar* toggle = dbusmenu_menuitem_property_get(item, DBUSMENU_MENUITEM_PROP_TOGGLE_TYPE);
    const bool radio = g_strcmp0(toggle, DBUSMENU_MENUITEM_TOGGLE_RADIO) == 0;
    const bool check = g_strcmp0(toggle, DBUSMENU_MENUITEM_TOGGLE_CHECK) == 0;
    if (g_strcmp0(dbusmenu_menuitem_property_get(item, DBUSMENU_MENUITEM_PROP_CHILD_DISPLAY),
                  DBUSMENU_MENUITEM_CHILD_DISPLAY_SUBMENU) == 0) {
      GMenu* submenu = g_menu_new();
      appendMenuItems(submenu, item, actions);
      g_menu_item_set_submenu(entry, G_MENU_MODEL(submenu));
      g_object_unref(submenu);
      // GTK sets the state of the submenu action to whether the submenu is shown. It carries over
      // rebuilds, or GTK would see an open submenu as closed and open it again.
      name.insert(0, "s");
      GAction* previous = menu_actions_ ? g_action_map_lookup_action(
                                              G_ACTION_MAP(menu_actions_->gobj()), name.c_str())
                                        : nullptr;
      g_autoptr(GVariant) shown = previous != nullptr ? g_action_get_state(previous) : nullptr;
      GSimpleAction* action = g_simple_action_new_stateful(
          name.c_str(), nullptr, shown != nullptr ? shown : g_variant_new_boolean(FALSE));
      g_signal_connect(action, "change-state", G_CALLBACK(&Item::onSubmenuShown), this);
      g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
      g_object_unref(action);
      name.insert(0, "tray.");
      g_menu_item_set_attribute(entry, "submenu-action", "s", name.c_str());
    } else {
      // A boolean state shows a check mark, a string state equal to the item target a radio
      GSimpleAction* action = nullptr;
      if (radio) {
        action =
            g_simple_action_new_stateful(name.c_str(), G_VARIANT_TYPE_STRING,
                                         g_variant_new_string(menuItemToggled(item) ? "1" : "0"));
      } else if (check) {
        action = g_simple_action_new_stateful(name.c_str(), nullptr,
                                              g_variant_new_boolean(menuItemToggled(item)));
      } else {
        action = g_simple_action_new(name.c_str(), nullptr);
      }
      g_simple_action_set_enabled(action, menuItemFlag(item, DBUSMENU_MENUITEM_PROP_ENABLED));
      // With a handler, activating does not change the state: the item reports its new one
      g_signal_connect(action, "activate", G_CALLBACK(&Item::onMenuActivate), this);
      g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(action));
      g_object_unref(action);
      name.insert(0, "tray.");
      g_menu_item_set_action_and_target_value(entry, name.c_str(),
                                              radio ? g_variant_new_string("1") : nullptr);
    }

    GVariant* shortcut =
        dbusmenu_menuitem_property_get_variant(item, DBUSMENU_MENUITEM_PROP_SHORTCUT);
    if (shortcut != nullptr && g_variant_is_of_type(shortcut, G_VARIANT_TYPE("aas")) &&
        g_variant_n_children(shortcut) > 0) {
      // [["Control", "Shift", "q"]] is "<Control><Shift>q"
      g_autoptr(GVariant) keys = g_variant_get_child_value(shortcut, 0);
      const gsize n_keys = g_variant_n_children(keys);
      std::string accel;
      for (gsize i = 0; i < n_keys; ++i) {
        const gchar* key = nullptr;
        g_variant_get_child(keys, i, "&s", &key);
        if (i + 1 < n_keys) {
          accel.append("<").append(key).append(">");
        } else {
          accel.append(key);
        }
      }
      g_menu_item_set_attribute(entry, "accel", "s", accel.c_str());
    }

    // Like libdbusmenu-gtk, which shows icons on standard items only, not on check or radio ones
    if (!radio && !check) {
      const gchar* icon_name =
          dbusmenu_menuitem_property_get(item, DBUSMENU_MENUITEM_PROP_ICON_NAME);
      GVariant* icon_data =
          dbusmenu_menuitem_property_get_variant(item, DBUSMENU_MENUITEM_PROP_ICON_DATA);
      GIcon* icon = nullptr;
      // A name is looked up in the theme directories, a path could name any file
      if (icon_name != nullptr && *icon_name != '\0' && strchr(icon_name, '/') == nullptr &&
          strcmp(icon_name, DBUSMENU_MENUITEM_ICON_NAME_BLANK) != 0) {
        icon = g_themed_icon_new(icon_name);
      } else if (icon_data != nullptr &&
                 g_variant_is_of_type(icon_data, G_VARIANT_TYPE_BYTESTRING) &&
                 isAcceptedPng(static_cast<const guint8*>(g_variant_get_data(icon_data)),
                               g_variant_get_size(icon_data), MENU_ICON_MAX_PIXELS)) {
        g_autoptr(GBytes) bytes = g_variant_get_data_as_bytes(icon_data);
        icon = g_bytes_icon_new(bytes);
      }
      if (icon != nullptr) {
        g_menu_item_set_icon(entry, icon);
        g_object_unref(icon);
      }
    }

    g_menu_append_item(section, entry);
    g_object_unref(entry);
  }
  if (g_menu_model_get_n_items(G_MENU_MODEL(section)) > 0) {
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(section));
  }
  g_object_unref(section);
}

void Item::onMenuActivate(GSimpleAction* action, GVariant* /*parameter*/, Item* self) {
  DbusmenuMenuitem* root = dbusmenu_client_get_root(self->dbus_menu);
  if (root == nullptr) {
    return;
  }
  // Looked up by id: the item may have left the tree since the menu was built
  DbusmenuMenuitem* item =
      dbusmenu_menuitem_find_id(root, std::atoi(g_action_get_name(G_ACTION(action))));
  if (item != nullptr) {
    dbusmenu_menuitem_handle_event(item, DBUSMENU_MENUITEM_EVENT_ACTIVATED, g_variant_new_int32(0),
                                   GDK_CURRENT_TIME);
  }
}

void Item::onSubmenuShown(GSimpleAction* action, GVariant* value, Item* self) {
  // GTK asks again for the state it is setting, from the state change notification
  g_autoptr(GVariant) state = g_action_get_state(G_ACTION(action));
  if (g_variant_equal(state, value)) {
    return;
  }
  g_simple_action_set_state(action, value);
  DbusmenuMenuitem* root = dbusmenu_client_get_root(self->dbus_menu);
  if (root == nullptr) {
    return;
  }
  // Named "s<id>"
  DbusmenuMenuitem* item =
      dbusmenu_menuitem_find_id(root, std::atoi(g_action_get_name(G_ACTION(action)) + 1));
  if (item == nullptr) {
    return;
  }
  // Like libdbusmenu-gtk, without waiting for the about-to-show reply
  if (g_variant_get_boolean(value)) {
    dbusmenu_menuitem_send_about_to_show(item, nullptr, nullptr);
    dbusmenu_menuitem_handle_event(item, DBUSMENU_MENUITEM_EVENT_OPENED, nullptr, GDK_CURRENT_TIME);
  } else {
    dbusmenu_menuitem_handle_event(item, DBUSMENU_MENUITEM_EVENT_CLOSED, nullptr, GDK_CURRENT_TIME);
  }
}

void Item::handleClick(int n_press, double x, double y) {
  if (!proxy_) {
    return;
  }
  const guint button = click_->get_current_button();
  const bool menu_button = (button == 1 && item_is_menu) || button == 3;
  if (!menu_button && button != 1 && button != 2) {
    return;
  }
  // Surface coordinates, like the GTK3 root coordinates of a Wayland surface
  double surface_x = 0;
  double surface_y = 0;
  auto& window = const_cast<Bar&>(bar_).window;
  gtk_native_get_surface_transform(GTK_NATIVE(window.gobj()), &surface_x, &surface_y);
  const auto point = event_box.compute_point(window, Gdk::Graphene::Point(x, y))
                         .value_or(Gdk::Graphene::Point(x, y));
  auto parameters = Glib::VariantContainerBase::create_tuple(
      {Glib::Variant<int>::create(point.get_x() + surface_x + bar_.x_global),
       Glib::Variant<int>::create(point.get_y() + surface_y + bar_.y_global)});

  // A double (triple) click is handled as a press then a double (triple) press, like in GTK3
  for (int i = (n_press - 1) % 3 == 0 ? 1 : 2; i > 0; --i) {
    if (menu_button) {
      makeMenu();
      // Some items advertise a Menu path without serving com.canonical.dbusmenu (e.g. Steam), so
      // the client never gets a layout root. Use the item's own ContextMenu for those instead of
      // popping up an empty menu.
      if (hasMenuLayout()) {
        // Like libdbusmenu-gtk, without waiting for the reply: an updated layout shows up live
        dbusmenu_menuitem_send_about_to_show(dbusmenu_client_get_root(dbus_menu), nullptr, nullptr);
        if (gtk_menu == nullptr) {
          // Nested submenus open beside their item, like cascading GTK3 menus
          gtk_menu = new Gtk::PopoverMenu({}, Gtk::PopoverMenu::Flags::NESTED);
          gtk_menu->set_has_arrow(false);
          gtk_menu->set_halign(Gtk::Align::START);
          gtk_menu->set_parent(event_box);
          util::unhover_menu_ancestors(GTK_WIDGET(gtk_menu->gobj()));
          gtk_menu->signal_map().connect(
              [this] { util::update_menu_items(GTK_WIDGET(gtk_menu->gobj()), true); });
        }
        if (menu_dirty_) {
          menu_rebuild_.disconnect();
          rebuildMenu();
        }
        gtk_menu->set_pointing_to(Gdk::Rectangle(static_cast<int>(x), static_cast<int>(y), 1, 1));
        gtk_menu->popup();
      } else {
        proxy_->call("ContextMenu", parameters);
      }
    } else if (button == 1) {
      proxy_->call("Activate", parameters);
    } else {
      proxy_->call("SecondaryActivate", parameters);
    }
  }
  click_->set_state(Gtk::EventSequenceState::CLAIMED);
}

bool Item::handleScroll(double dx, double dy) {
  if (!proxy_) {
    return false;
  }
  // Scaled to the smooth deltas GTK3 gave on Wayland, which smooth-scrolling-threshold is tuned
  // for: 1.5 per wheel detent and a tenth of a surface pixel.
  const double factor = scroll_->get_unit() == Gdk::ScrollUnit::WHEEL ? 1.5 : 0.1;
  distance_scrolled_x_ += dx * factor;
  distance_scrolled_y_ += dy * factor;
  int scroll_x = 0;
  int scroll_y = 0;
  // check against the configured threshold and ensure that the absolute value >= 1
  if (distance_scrolled_x_ > scroll_threshold_) {
    scroll_x = (int)lround(std::max(distance_scrolled_x_, 1.0));
    distance_scrolled_x_ = 0;
  } else if (distance_scrolled_x_ < -scroll_threshold_) {
    scroll_x = (int)lround(std::min(distance_scrolled_x_, -1.0));
    distance_scrolled_x_ = 0;
  }
  if (distance_scrolled_y_ > scroll_threshold_) {
    scroll_y = (int)lround(std::max(distance_scrolled_y_, 1.0));
    distance_scrolled_y_ = 0;
  } else if (distance_scrolled_y_ < -scroll_threshold_) {
    scroll_y = (int)lround(std::min(distance_scrolled_y_, -1.0));
    distance_scrolled_y_ = 0;
  }
  if (scroll_x != 0) {
    auto parameters = Glib::VariantContainerBase::create_tuple(
        {Glib::Variant<int>::create(scroll_x), Glib::Variant<Glib::ustring>::create("horizontal")});
    proxy_->call("Scroll", parameters);
  }
  if (scroll_y != 0) {
    auto parameters = Glib::VariantContainerBase::create_tuple(
        {Glib::Variant<int>::create(scroll_y), Glib::Variant<Glib::ustring>::create("vertical")});
    proxy_->call("Scroll", parameters);
  }
  return true;
}

}  // namespace waybar::modules::SNI
