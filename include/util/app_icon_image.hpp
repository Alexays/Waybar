#pragma once

#include <giomm/desktopappinfo.h>
#include <gtkmm/image.h>

#include <optional>
#include <string>

#include "util/icon_loader.hpp"

/*
 * Gtk::Image that rasterizes an application icon for the scale factor of the widget it is in and
 * rasterizes it again when that factor changes. A bar on a fractionally scaled output renders
 * into a larger buffer, and an icon loaded at the logical size would be stretched into that
 * buffer and then scaled down again to the monitor.
 */
class AppIconImage : public Gtk::Image {
 public:
  AppIconImage();

  // The loader must outlive the image; modules own one and the widgets are its children
  IconLoader::LoadResult setAppInfo(const IconLoader& loader,
                                    Glib::RefPtr<Gio::DesktopAppInfo> app_info, int size);
  IconLoader::LoadResult setAppId(const IconLoader& loader, const std::string& app_id, int size);

 private:
  enum class Source { APP_INFO, APP_ID };

  IconLoader::LoadResult reload();
  void onScaleChanged();

  const IconLoader* loader_ = nullptr;
  Glib::RefPtr<Gio::DesktopAppInfo> app_info_;
  std::string app_id_;
  int size_ = 0;
  std::optional<Source> source_;
};
