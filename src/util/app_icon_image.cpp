#include "util/app_icon_image.hpp"

#include <spdlog/spdlog.h>

#include <utility>

AppIconImage::AppIconImage() {
  // Gtk::Widget is a sigc::trackable, so the slot goes away with the image
  property_scale_factor().signal_changed().connect(
      sigc::mem_fun(*this, &AppIconImage::onScaleChanged));
}

IconLoader::LoadResult AppIconImage::setAppInfo(const IconLoader& loader,
                                                Glib::RefPtr<Gio::DesktopAppInfo> app_info,
                                                int size) {
  loader_ = &loader;
  app_info_ = std::move(app_info);
  app_id_.clear();
  size_ = size;
  source_ = Source::APP_INFO;
  return reload();
}

IconLoader::LoadResult AppIconImage::setAppId(const IconLoader& loader, const std::string& app_id,
                                              int size) {
  loader_ = &loader;
  app_info_.reset();
  app_id_ = app_id;
  size_ = size;
  source_ = Source::APP_ID;
  return reload();
}

IconLoader::LoadResult AppIconImage::reload() {
  if (loader_ == nullptr || !source_.has_value()) {
    return IconLoader::LoadResult::FAILED;
  }
  switch (source_.value()) {
    case Source::APP_INFO:
      return loader_->image_load_icon(*this, app_info_, size_);
    case Source::APP_ID:
      return loader_->image_load_icon(*this, app_id_, size_);
  }
  return IconLoader::LoadResult::FAILED;
}

void AppIconImage::onScaleChanged() {
  if (loader_ == nullptr || !source_.has_value()) {
    return;
  }
  spdlog::debug("Scale factor changed to {}, reloading app icon", get_scale_factor());
  reload();
}
