#include "modules/cosmic/workspace_thumbnails.hpp"

#include <fcntl.h>
#include <gdk/gdkwayland.h>
#include <gtkmm.h>
#include <spdlog/spdlog.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>

#include "client.hpp"

namespace waybar::modules::cosmic {

namespace {

Gtk::Align parse_align(const Json::Value& v, Gtk::Align fallback) {
  if (!v.isString()) {
    return fallback;
  }
  const auto& s = v.asString();
  if (s == "start") return Gtk::ALIGN_START;
  if (s == "center") return Gtk::ALIGN_CENTER;
  if (s == "end") return Gtk::ALIGN_END;
  spdlog::warn("[cosmic/workspaces]: unknown alignment '{}', expected start/center/end", s);
  return fallback;
}

Gtk::Orientation opts_orientation(const ThumbnailOptions& opts) {
  return opts.icons_orientation == IconsOrientation::Vertical ? Gtk::ORIENTATION_VERTICAL
                                                              : Gtk::ORIENTATION_HORIZONTAL;
}

Gtk::Align icons_align_to_gtk(IconsAlign align) {
  switch (align) {
    case IconsAlign::Center:
      return Gtk::ALIGN_CENTER;
    case IconsAlign::End:
      return Gtk::ALIGN_END;
    default:
      return Gtk::ALIGN_START;
  }
}

// Orientation + overlay alignment for an embedded icon box anchored to one edge of the
// thumbnail, positioned along that edge per `align` (e.g. edge=bottom, align=end => bottom-right).
struct EdgePlacement {
  Gtk::Orientation orientation;
  Gtk::Align halign;
  Gtk::Align valign;
};

EdgePlacement embedded_placement(IconsEdge edge, IconsAlign align) {
  const Gtk::Align cross = icons_align_to_gtk(align);
  switch (edge) {
    case IconsEdge::Top:
      return {Gtk::ORIENTATION_HORIZONTAL, cross, Gtk::ALIGN_START};
    case IconsEdge::Left:
      return {Gtk::ORIENTATION_VERTICAL, Gtk::ALIGN_START, cross};
    case IconsEdge::Right:
      return {Gtk::ORIENTATION_VERTICAL, Gtk::ALIGN_END, cross};
    case IconsEdge::Bottom:
    default:
      return {Gtk::ORIENTATION_HORIZONTAL, cross, Gtk::ALIGN_END};
  }
}

void replace_all(std::string& s, const std::string& from, const std::string& to) {
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.size(), to);
    pos += to.size();
  }
}

std::string format_workspace_label(const std::string& fmt, const std::string& name, int number) {
  std::string out = fmt;
  replace_all(out, "{name}", name);
  replace_all(out, "{number}", std::to_string(number));
  return out;
}

}  // namespace

/* ---- Thumbnail: per-workspace preview, either live screencopy or a plain styled rectangle ---- */

Thumbnail::Thumbnail(WorkspaceThumbnails& owner, ext_workspace_handle_v1* workspace)
    : owner_(owner),
      workspace_(workspace),
      item_box_(owner.bar_orientation(), 0),
      icons_box_(opts_orientation(owner.options()), owner.options().icons_spacing) {
  const auto& opts = owner_.options();

  convert_dispatcher_.connect(sigc::mem_fun(*this, &Thumbnail::on_convert_done));

  item_box_.get_style_context()->add_class("workspace");
  // Gtk::Box otherwise stretches children across the cross-axis regardless of
  // pack_start's fill flag, sizing the thumbnail (and its highlight) to the
  // bar's own width/height instead of its own configured size.
  item_box_.set_halign(Gtk::ALIGN_CENTER);
  item_box_.set_valign(Gtk::ALIGN_CENTER);

  area_.set_size_request(owner_.thumbnail_width(), owner_.thumbnail_height());
  area_.get_style_context()->add_class("thumbnail");
  area_.add_events(Gdk::BUTTON_PRESS_MASK);
  area_.signal_draw().connect(sigc::mem_fun(*this, &Thumbnail::on_draw));
  area_.signal_button_press_event().connect(sigc::mem_fun(*this, &Thumbnail::on_button_press));
  overlay_.add(area_);
  overlay_.set_halign(Gtk::ALIGN_CENTER);
  overlay_.set_valign(Gtk::ALIGN_CENTER);

  thumbnail_label_.get_style_context()->add_class("thumbnail-label");
  // Overlay children (unlike area_ below them) have no window of their own and would
  // otherwise just swallow the click without ever handing it to area_'s handler, so
  // give the label an (invisible) input window of its own to activate the workspace too.
  thumbnail_label_box_.set_visible_window(false);
  thumbnail_label_box_.add(thumbnail_label_);
  thumbnail_label_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  thumbnail_label_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &Thumbnail::on_button_press));
  thumbnail_label_box_.set_halign(opts.thumbnail_label_halign);
  thumbnail_label_box_.set_valign(opts.thumbnail_label_valign);
  if (opts.thumbnail_label_show) {
    overlay_.add_overlay(thumbnail_label_box_);
  }

  adjacent_label_.get_style_context()->add_class("label");
  adjacent_label_box_.set_visible_window(false);
  adjacent_label_box_.add(adjacent_label_);
  adjacent_label_box_.add_events(Gdk::BUTTON_PRESS_MASK);
  adjacent_label_box_.signal_button_press_event().connect(
      sigc::mem_fun(*this, &Thumbnail::on_button_press));

  icons_box_.get_style_context()->add_class("icons");
  const bool icons_packed = opts.icons_show && opts.icons_position != IconsPosition::Embedded;

  // Builds item_box_'s children in order; a non-embedded icons-position is resolved relative
  // to whichever of the label/thumbnail precedes the other (per label-position), so e.g.
  // "afterlabel" and "beforethumbnail" land in the same slot when the label comes first, and
  // (when label-show is false) "beforelabel"/"afterlabel" naturally collapse to the thumbnail's
  // own edge since there's no label to anchor to.
  if (opts.label_position == LabelPosition::Before) {
    if (icons_packed && opts.icons_position == IconsPosition::BeforeLabel) {
      item_box_.pack_start(icons_box_, false, false);
    }
    if (opts.label_show) {
      item_box_.pack_start(adjacent_label_box_, false, false);
    }
    if (icons_packed && (opts.icons_position == IconsPosition::AfterLabel ||
                        opts.icons_position == IconsPosition::BeforeThumbnail)) {
      item_box_.pack_start(icons_box_, false, false);
    }
    item_box_.pack_start(overlay_, false, false);
    if (icons_packed && opts.icons_position == IconsPosition::AfterThumbnail) {
      item_box_.pack_start(icons_box_, false, false);
    }
  } else {
    if (icons_packed && opts.icons_position == IconsPosition::BeforeThumbnail) {
      item_box_.pack_start(icons_box_, false, false);
    }
    item_box_.pack_start(overlay_, false, false);
    if (icons_packed && (opts.icons_position == IconsPosition::AfterThumbnail ||
                        opts.icons_position == IconsPosition::BeforeLabel)) {
      item_box_.pack_start(icons_box_, false, false);
    }
    if (opts.label_show) {
      item_box_.pack_start(adjacent_label_box_, false, false);
    }
    if (icons_packed && opts.icons_position == IconsPosition::AfterLabel) {
      item_box_.pack_start(icons_box_, false, false);
    }
  }

  if (opts.icons_show && opts.icons_position == IconsPosition::Embedded) {
    const auto placement = embedded_placement(opts.icons_edge, opts.icons_align);
    icons_box_.set_orientation(placement.orientation);
    icons_box_.set_halign(placement.halign);
    icons_box_.set_valign(placement.valign);
    overlay_.add_overlay(icons_box_);
  }

  refresh_style();
  refresh_icons();

  if (opts.display == DisplayMode::Live) {
    start_session();
  }
}

Thumbnail::~Thumbnail() { release_session(); }

void Thumbnail::resize(int width, int height) { area_.set_size_request(width, height); }

void Thumbnail::refresh_style() {
  const auto& opts = owner_.options();
  const auto* meta = owner_.meta_for(workspace_);
  const std::string name = meta ? meta->name : std::string();
  const int number = owner_.number_for(workspace_);
  const std::string text = format_workspace_label(opts.thumbnail_label, name, number);

  thumbnail_label_.set_text(text);
  adjacent_label_.set_text(text);

  auto style = area_.get_style_context();
  if (opts.highlight_show && owner_.is_effectively_active(workspace_)) {
    style->add_class("active");
  } else {
    style->remove_class("active");
  }
  area_.queue_draw();
}

void Thumbnail::refresh_icons() {
  const auto& opts = owner_.options();
  if (!opts.icons_show) {
    icons_box_.hide();
    return;
  }

  for (auto* child : icons_box_.get_children()) {
    icons_box_.remove(*child);
  }

  for (const auto* toplevel : owner_.icons_for(workspace_)) {
    auto* btn = Gtk::make_managed<Gtk::Button>();
    btn->set_relief(Gtk::RELIEF_NONE);
    btn->get_style_context()->add_class("icon");
    btn->set_tooltip_text(toplevel->app_id);

    auto* img = Gtk::make_managed<Gtk::Image>();
    auto app_info = IconLoader::get_app_info_from_app_id_list(toplevel->app_id);
    owner_.icon_loader().image_load_icon(*img, app_info, opts.icons_size);
    btn->add(*img);

    zcosmic_toplevel_handle_v1* cosmic_handle = toplevel->cosmic;
    btn->signal_clicked().connect(
        [this, cosmic_handle]() { owner_.activate_toplevel(cosmic_handle); });

    icons_box_.pack_start(*btn, false, false);
  }

  icons_box_.show_all();
}

static void session_handle_buffer_size(void* data, ext_image_copy_capture_session_v1*,
                                       uint32_t width, uint32_t height) {
  static_cast<Thumbnail*>(data)->handle_buffer_size(width, height);
}

static void session_handle_shm_format(void* data, ext_image_copy_capture_session_v1*,
                                      uint32_t format) {
  static_cast<Thumbnail*>(data)->handle_shm_format(format);
}

static void session_handle_dmabuf_device(void*, ext_image_copy_capture_session_v1*, wl_array*) {}

static void session_handle_dmabuf_format(void*, ext_image_copy_capture_session_v1*, uint32_t,
                                         wl_array*) {}

static void session_handle_done(void* data, ext_image_copy_capture_session_v1*) {
  static_cast<Thumbnail*>(data)->handle_constraints_done();
}

static void session_handle_stopped(void* data, ext_image_copy_capture_session_v1*) {
  static_cast<Thumbnail*>(data)->handle_session_stopped();
}

static const struct ext_image_copy_capture_session_v1_listener session_impl = {
    .buffer_size = session_handle_buffer_size,
    .shm_format = session_handle_shm_format,
    .dmabuf_device = session_handle_dmabuf_device,
    .dmabuf_format = session_handle_dmabuf_format,
    .done = session_handle_done,
    .stopped = session_handle_stopped,
};

static void frame_handle_transform(void*, ext_image_copy_capture_frame_v1*, uint32_t) {}

static void frame_handle_damage(void*, ext_image_copy_capture_frame_v1*, int32_t, int32_t, int32_t,
                                int32_t) {}

static void frame_handle_presentation_time(void*, ext_image_copy_capture_frame_v1*, uint32_t,
                                           uint32_t, uint32_t) {}

static void frame_handle_ready(void* data, ext_image_copy_capture_frame_v1*) {
  static_cast<Thumbnail*>(data)->handle_frame_ready();
}

static void frame_handle_failed(void* data, ext_image_copy_capture_frame_v1*, uint32_t) {
  static_cast<Thumbnail*>(data)->handle_frame_failed();
}

static const struct ext_image_copy_capture_frame_v1_listener frame_impl = {
    .transform = frame_handle_transform,
    .damage = frame_handle_damage,
    .presentation_time = frame_handle_presentation_time,
    .ready = frame_handle_ready,
    .failed = frame_handle_failed,
};

void Thumbnail::start_session() {
  auto* source_mgr = owner_.source_manager();
  auto* capture_mgr = owner_.capture_manager();
  if (!source_mgr || !capture_mgr) {
    return;
  }

  source_ = zcosmic_workspace_image_capture_source_manager_v1_create_source(source_mgr, workspace_);
  if (!source_) {
    spdlog::error("[cosmic/workspaces]: Failed to create capture source for workspace");
    return;
  }

  session_ = ext_image_copy_capture_manager_v1_create_session(capture_mgr, source_, 0);
  if (!session_) {
    spdlog::error("[cosmic/workspaces]: Failed to create capture session for workspace");
    return;
  }
  ext_image_copy_capture_session_v1_add_listener(session_, &session_impl, this);
  start_worker();
}

void Thumbnail::handle_buffer_size(uint32_t width, uint32_t height) {
  buf_width_ = width;
  buf_height_ = height;
}

namespace {
// cosmic-comp only advertises the *BGR8888 shm formats (matching the *BGR
// byte order cosmic-client-toolkit assumes), so ARGB8888 is rarely, if ever,
// actually offered; rank by alpha support rather than assuming ARGB8888 wins.
int shm_format_rank(uint32_t format) {
  switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
      return 4;
    case WL_SHM_FORMAT_ABGR8888:
      return 3;
    case WL_SHM_FORMAT_XRGB8888:
      return 2;
    case WL_SHM_FORMAT_XBGR8888:
      return 1;
    default:
      return 0;
  }
}
}  // namespace

void Thumbnail::handle_shm_format(uint32_t format) {
  if (!have_shm_format_ || shm_format_rank(format) > shm_format_rank(shm_format_)) {
    shm_format_ = format;
    have_shm_format_ = true;
  }
}

void Thumbnail::handle_constraints_done() {
  if (!ensure_buffer()) {
    return;
  }
  if (!staggered_) {
    staggered_ = true;
    // Spread workspaces' first capture across the interval instead of all starting at once,
    // which would otherwise keep them in lockstep (each reschedules itself a fixed `interval()`
    // after its own frame arrives) and re-create the same main-thread stall on every tick.
    const int index = std::max(0, owner_.number_for(workspace_) - 1);
    const int stagger_ms =
        static_cast<int>(index * owner_.interval().count() / std::max<size_t>(1, owner_.thumbnail_count()));
    timer_conn_.disconnect();
    timer_conn_ = Glib::signal_timeout().connect(
        [this]() {
          request_capture();
          return false;
        },
        stagger_ms);
    return;
  }
  request_capture();
}

void Thumbnail::handle_session_stopped() { release_session(); }

bool Thumbnail::ensure_buffer() {
  if (buffer_ && alloc_width_ == buf_width_ && alloc_height_ == buf_height_) {
    return true;
  }
  release_buffer();

  wl_shm* shm = owner_.shm();
  if (!shm || !have_shm_format_ || buf_width_ == 0 || buf_height_ == 0) {
    return false;
  }

  const size_t stride = static_cast<size_t>(buf_width_) * 4;
  const size_t size = stride * buf_height_;

  int fd = memfd_create("waybar-cosmic-thumbnail", MFD_CLOEXEC);
  if (fd < 0) {
    spdlog::error("[cosmic/workspaces]: memfd_create failed: {}", strerror(errno));
    return false;
  }
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    spdlog::error("[cosmic/workspaces]: ftruncate failed: {}", strerror(errno));
    close(fd);
    return false;
  }
  void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    spdlog::error("[cosmic/workspaces]: mmap failed: {}", strerror(errno));
    close(fd);
    return false;
  }

  wl_shm_pool* pool = wl_shm_create_pool(shm, fd, static_cast<int32_t>(size));
  close(fd);
  wl_buffer* buffer = wl_shm_pool_create_buffer(
      pool, 0, static_cast<int32_t>(buf_width_), static_cast<int32_t>(buf_height_),
      static_cast<int32_t>(stride), static_cast<wl_shm_format>(shm_format_));
  wl_shm_pool_destroy(pool);

  buffer_ = buffer;
  buffer_data_ = static_cast<uint8_t*>(data);
  buffer_stride_ = stride;
  buffer_len_ = size;
  alloc_width_ = buf_width_;
  alloc_height_ = buf_height_;
  return true;
}

void Thumbnail::release_buffer() {
  // Must not unmap buffer_data_ while the worker is still reading it for the current job.
  {
    std::unique_lock<std::mutex> lock(job_mutex_);
    buffer_free_cv_.wait(lock, [this] { return !buffer_busy_; });
  }
  if (buffer_) {
    wl_buffer_destroy(buffer_);
    buffer_ = nullptr;
  }
  if (buffer_data_) {
    munmap(buffer_data_, buffer_len_);
    buffer_data_ = nullptr;
    buffer_len_ = 0;
  }
  alloc_width_ = 0;
  alloc_height_ = 0;
}

void Thumbnail::request_capture() {
  if (!session_ || !buffer_ || capture_in_flight_ || convert_in_flight_ || frame_) {
    return;
  }
  if (!owner_.try_acquire_capture_slot()) {
    // Every capture slot (compositor readback + background conversion) is in use by some
    // other thumbnail; retry shortly rather than waiting out this thumbnail's own interval.
    constexpr int kCaptureSlotRetryMs = 20;
    timer_conn_.disconnect();
    timer_conn_ = Glib::signal_timeout().connect(
        [this]() {
          request_capture();
          return false;
        },
        kCaptureSlotRetryMs);
    return;
  }
  frame_ = ext_image_copy_capture_session_v1_create_frame(session_);
  if (!frame_) {
    owner_.release_capture_slot();
    return;
  }
  ext_image_copy_capture_frame_v1_add_listener(frame_, &frame_impl, this);
  ext_image_copy_capture_frame_v1_attach_buffer(frame_, buffer_);
  ext_image_copy_capture_frame_v1_damage_buffer(frame_, 0, 0, static_cast<int32_t>(buf_width_),
                                                static_cast<int32_t>(buf_height_));
  ext_image_copy_capture_frame_v1_capture(frame_);
  capture_in_flight_ = true;
}

void Thumbnail::handle_frame_ready() {
  capture_in_flight_ = false;
  if (frame_) {
    ext_image_copy_capture_frame_v1_destroy(frame_);
    frame_ = nullptr;
  }

  if (buffer_data_ == nullptr || buf_width_ == 0 || buf_height_ == 0) {
    schedule_next_capture();
    return;
  }

  // Hand the full-resolution copy/format-swap to the persistent worker thread so it can't
  // stall the GTK main loop; request_capture()/release_buffer() hold off touching buffer_
  // until buffer_busy_/convert_in_flight_ clear, so the worker's read below never races a
  // write into the same memory.
  convert_in_flight_ = true;
  {
    std::lock_guard<std::mutex> lock(job_mutex_);
    buffer_busy_ = true;
    pending_job_ = ConvertJob{buffer_data_, buffer_stride_, buf_width_, buf_height_, shm_format_};
    has_job_ = true;
  }
  job_cv_.notify_one();
}

namespace {
Cairo::RefPtr<Cairo::ImageSurface> convert_frame(const uint8_t* buffer_data, size_t buffer_stride,
                                                  uint32_t width, uint32_t height,
                                                  uint32_t shm_format) {
  const bool has_alpha =
      shm_format == WL_SHM_FORMAT_ARGB8888 || shm_format == WL_SHM_FORMAT_ABGR8888;
  // *BGR8888 stores red/blue swapped relative to Cairo's native *RGB32 memory layout.
  const bool swap_rb = shm_format == WL_SHM_FORMAT_ABGR8888 || shm_format == WL_SHM_FORMAT_XBGR8888;
  const auto cairo_format = has_alpha ? Cairo::FORMAT_ARGB32 : Cairo::FORMAT_RGB24;

  auto surface = Cairo::ImageSurface::create(cairo_format, static_cast<int>(width),
                                             static_cast<int>(height));
  const size_t dst_stride = static_cast<size_t>(surface->get_stride());
  unsigned char* dst = surface->get_data();
  const size_t copy_len = std::min(dst_stride, buffer_stride);

  if (!swap_rb) {
    for (uint32_t y = 0; y < height; ++y) {
      std::memcpy(dst + y * dst_stride, buffer_data + y * buffer_stride, copy_len);
    }
  } else {
    // Swap R/B as whole 32-bit words (one load/mask/store per pixel) rather than four
    // separate byte stores - cheaper and easier for the compiler to auto-vectorize.
    const uint32_t pixels_per_row = static_cast<uint32_t>(copy_len / 4);
    for (uint32_t y = 0; y < height; ++y) {
      const auto* src_row = reinterpret_cast<const uint32_t*>(buffer_data + y * buffer_stride);
      auto* dst_row = reinterpret_cast<uint32_t*>(dst + y * dst_stride);
      for (uint32_t x = 0; x < pixels_per_row; ++x) {
        const uint32_t px = src_row[x];
        dst_row[x] = (px & 0xFF00FF00u) | ((px & 0x00FF0000u) >> 16) | ((px & 0x000000FFu) << 16);
      }
    }
  }
  surface->mark_dirty();
  return surface;
}
}  // namespace

void Thumbnail::start_worker() {
  if (worker_thread_.joinable()) {
    return;
  }
  stop_worker_ = false;
  worker_thread_ = std::thread(&Thumbnail::worker_main, this);
}

void Thumbnail::stop_worker() {
  if (!worker_thread_.joinable()) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(job_mutex_);
    stop_worker_ = true;
  }
  job_cv_.notify_all();
  worker_thread_.join();
}

void Thumbnail::worker_main() {
  for (;;) {
    ConvertJob job;
    {
      std::unique_lock<std::mutex> lock(job_mutex_);
      job_cv_.wait(lock, [this] { return has_job_ || stop_worker_; });
      if (stop_worker_ && !has_job_) {
        return;
      }
      job = pending_job_;
      has_job_ = false;
    }

    auto surface = convert_frame(job.data, job.stride, job.width, job.height, job.format);

    {
      std::lock_guard<std::mutex> lock(job_mutex_);
      buffer_busy_ = false;
    }
    buffer_free_cv_.notify_all();

    {
      std::lock_guard<std::mutex> lock(convert_mutex_);
      pending_surface_ = surface;
    }
    convert_dispatcher_.emit();
  }
}

void Thumbnail::on_convert_done() {
  Cairo::RefPtr<Cairo::ImageSurface> surface;
  {
    std::lock_guard<std::mutex> lock(convert_mutex_);
    surface = pending_surface_;
    pending_surface_.clear();
  }
  convert_in_flight_ = false;
  owner_.release_capture_slot();
  if (surface) {
    surface_ = surface;
    area_.queue_draw();
  }
  schedule_next_capture();
}

void Thumbnail::schedule_next_capture() {
  timer_conn_.disconnect();
  timer_conn_ = Glib::signal_timeout().connect(sigc::mem_fun(*this, &Thumbnail::on_capture_timeout),
                                               owner_.interval().count());
}

void Thumbnail::handle_frame_failed() {
  capture_in_flight_ = false;
  if (frame_) {
    ext_image_copy_capture_frame_v1_destroy(frame_);
    frame_ = nullptr;
  }
  owner_.release_capture_slot();
  schedule_next_capture();
}

void Thumbnail::release_session() {
  timer_conn_.disconnect();
  if (frame_) {
    ext_image_copy_capture_frame_v1_destroy(frame_);
    frame_ = nullptr;
  }
  if (session_) {
    ext_image_copy_capture_session_v1_destroy(session_);
    session_ = nullptr;
  }
  if (source_) {
    ext_image_capture_source_v1_destroy(source_);
    source_ = nullptr;
  }
  // A capture/convert in flight (e.g. the workspace was deleted mid-capture) holds a
  // slot that on_convert_done()/handle_frame_failed() will never now run to release.
  if (capture_in_flight_ || convert_in_flight_) {
    owner_.release_capture_slot();
  }
  stop_worker();
  release_buffer();
  capture_in_flight_ = false;
  convert_in_flight_ = false;
  have_shm_format_ = false;
  buf_width_ = buf_height_ = 0;
}

bool Thumbnail::on_capture_timeout() {
  request_capture();
  return false;
}

bool Thumbnail::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
  const auto& opts = owner_.options();
  const auto allocation = area_.get_allocation();
  const double w = allocation.get_width();
  const double h = allocation.get_height();
  auto style = area_.get_style_context();

  // Standard mode's rectangle (and the active-state fill, if themed) comes
  // entirely from CSS; live mode paints over this with the captured surface.
  style->render_background(cr, 0, 0, w, h);

  if (opts.display == DisplayMode::Live && surface_) {
    cr->save();
    // Crop the *source* surface to the clean region, then scale that region to
    // fill the widget - not a widget-local mask, which would just leave a
    // barely-visible border instead of actually excluding the dead zones.
    double src_x = 0, src_y = 0;
    double src_w = surface_->get_width(), src_h = surface_->get_height();
    if (opts.crop_dead_zones != CropMode::None) {
      const CropRect crop = owner_.dead_zone_crop(workspace_, src_w, src_h);
      src_x = crop.x;
      src_y = crop.y;
      src_w = crop.width;
      src_h = crop.height;
    }
    const double scale = std::min(w / src_w, h / src_h);
    cr->translate((w - src_w * scale) / 2.0, (h - src_h * scale) / 2.0);
    cr->scale(scale, scale);
    cr->rectangle(0, 0, src_w, src_h);
    cr->clip();
    cr->set_source(surface_, -src_x, -src_y);
    cr->paint();
    cr->restore();
  }

  style->render_frame(cr, 0, 0, w, h);

  // Drawn manually (rather than via CSS border-width) so the line thickness is
  // controlled by highlight-line-size; the colour still comes from CSS.
  if (opts.highlight_show && opts.highlight_line_size > 0 &&
      owner_.is_effectively_active(workspace_)) {
    const double lw = opts.highlight_line_size;
    const Gdk::RGBA color = style->get_border_color(Gtk::STATE_FLAG_NORMAL);
    cr->save();
    cr->set_line_width(lw);
    cr->set_source_rgba(color.get_red(), color.get_green(), color.get_blue(), color.get_alpha());
    cr->rectangle(lw / 2.0, lw / 2.0, w - lw, h - lw);
    cr->stroke();
    cr->restore();
  }

  return true;
}

bool Thumbnail::on_button_press(GdkEventButton* event) {
  if (event->type == GDK_BUTTON_PRESS && event->button == 1) {
    owner_.request_activate(workspace_);
  }
  return true;
}

/* ---- WorkspaceThumbnails: module, registry and workspace listing ---- */

static void handle_global(void* data, wl_registry* registry, uint32_t name, const char* interface,
                          uint32_t version) {
  static_cast<WorkspaceThumbnails*>(data)->register_manager(registry, name, interface, version);
}

static void handle_global_remove(void*, wl_registry*, uint32_t) {}

static const wl_registry_listener registry_listener_impl = {
    .global = handle_global,
    .global_remove = handle_global_remove,
};

static void workspace_handle_id(void*, ext_workspace_handle_v1*, const char*) {}

static void workspace_handle_name(void* data, ext_workspace_handle_v1* handle, const char* name) {
  static_cast<WorkspaceThumbnails*>(data)->handle_workspace_name(handle, name ? name : "");
}

static void workspace_handle_coordinates(void*, ext_workspace_handle_v1*, wl_array*) {}

static void workspace_handle_state(void* data, ext_workspace_handle_v1* handle, uint32_t state) {
  static_cast<WorkspaceThumbnails*>(data)->handle_workspace_state(handle, state);
}

static void workspace_handle_capabilities(void*, ext_workspace_handle_v1*, uint32_t) {}

static void workspace_handle_removed(void* data, ext_workspace_handle_v1* handle) {
  static_cast<WorkspaceThumbnails*>(data)->handle_workspace_removed(handle);
}

static const struct ext_workspace_handle_v1_listener workspace_handle_impl = {
    .id = workspace_handle_id,
    .name = workspace_handle_name,
    .coordinates = workspace_handle_coordinates,
    .state = workspace_handle_state,
    .capabilities = workspace_handle_capabilities,
    .removed = workspace_handle_removed,
};

static void workspace_group_handle_capabilities(void*, ext_workspace_group_handle_v1*, uint32_t) {}

static void workspace_group_handle_output_enter(void* data, ext_workspace_group_handle_v1* group,
                                                wl_output* output) {
  static_cast<WorkspaceThumbnails*>(data)->handle_group_output_enter(group, output);
}

static void workspace_group_handle_output_leave(void* data, ext_workspace_group_handle_v1* group,
                                                wl_output* output) {
  static_cast<WorkspaceThumbnails*>(data)->handle_group_output_leave(group, output);
}

static void workspace_group_handle_workspace_enter(void* data, ext_workspace_group_handle_v1* group,
                                                   ext_workspace_handle_v1* ws) {
  static_cast<WorkspaceThumbnails*>(data)->handle_group_workspace_enter(group, ws);
}

static void workspace_group_handle_workspace_leave(void* data, ext_workspace_group_handle_v1* group,
                                                   ext_workspace_handle_v1* ws) {
  static_cast<WorkspaceThumbnails*>(data)->handle_group_workspace_leave(group, ws);
}

static void workspace_group_handle_removed(void* data, ext_workspace_group_handle_v1* group) {
  static_cast<WorkspaceThumbnails*>(data)->handle_group_removed(group);
}

static const struct ext_workspace_group_handle_v1_listener workspace_group_impl = {
    .capabilities = workspace_group_handle_capabilities,
    .output_enter = workspace_group_handle_output_enter,
    .output_leave = workspace_group_handle_output_leave,
    .workspace_enter = workspace_group_handle_workspace_enter,
    .workspace_leave = workspace_group_handle_workspace_leave,
    .removed = workspace_group_handle_removed,
};

static void workspace_manager_handle_group(void* data, ext_workspace_manager_v1*,
                                           ext_workspace_group_handle_v1* group) {
  static_cast<WorkspaceThumbnails*>(data)->handle_workspace_group_create(group);
}

static void workspace_manager_handle_workspace(void* data, ext_workspace_manager_v1*,
                                               ext_workspace_handle_v1* workspace) {
  static_cast<WorkspaceThumbnails*>(data)->handle_workspace_create(workspace);
}

static void workspace_manager_handle_done(void* data, ext_workspace_manager_v1*) {
  static_cast<WorkspaceThumbnails*>(data)->handle_manager_done();
}

static void workspace_manager_handle_finished(void* data, ext_workspace_manager_v1*) {
  static_cast<WorkspaceThumbnails*>(data)->handle_manager_finished();
}

static const struct ext_workspace_manager_v1_listener workspace_manager_impl = {
    .workspace_group = workspace_manager_handle_group,
    .workspace = workspace_manager_handle_workspace,
    .done = workspace_manager_handle_done,
    .finished = workspace_manager_handle_finished,
};

/* ---- App icons: ext-foreign-toplevel-list-v1 (app_id) + cosmic-toplevel-info
 * (workspace membership, via get_cosmic_toplevel) + cosmic-toplevel-management
 * (activation on click) */

static void toplevel_handle_closed(void* data, ext_foreign_toplevel_handle_v1* handle) {
  static_cast<WorkspaceThumbnails*>(data)->handle_toplevel_closed(handle);
}

static void toplevel_handle_done(void* data, ext_foreign_toplevel_handle_v1* handle) {
  static_cast<WorkspaceThumbnails*>(data)->handle_toplevel_done(handle);
}

static void toplevel_handle_title(void*, ext_foreign_toplevel_handle_v1*, const char*) {}

static void toplevel_handle_app_id(void* data, ext_foreign_toplevel_handle_v1* handle,
                                   const char* app_id) {
  static_cast<WorkspaceThumbnails*>(data)->handle_toplevel_app_id(handle, app_id ? app_id : "");
}

static void toplevel_handle_identifier(void*, ext_foreign_toplevel_handle_v1*, const char*) {}

static const struct ext_foreign_toplevel_handle_v1_listener toplevel_handle_impl = {
    .closed = toplevel_handle_closed,
    .done = toplevel_handle_done,
    .title = toplevel_handle_title,
    .app_id = toplevel_handle_app_id,
    .identifier = toplevel_handle_identifier,
};

static void toplevel_list_handle_toplevel(void* data, ext_foreign_toplevel_list_v1*,
                                          ext_foreign_toplevel_handle_v1* handle) {
  static_cast<WorkspaceThumbnails*>(data)->handle_toplevel_list_toplevel(handle);
}

static void toplevel_list_handle_finished(void* data, ext_foreign_toplevel_list_v1*) {
  static_cast<WorkspaceThumbnails*>(data)->handle_toplevel_list_finished();
}

static const struct ext_foreign_toplevel_list_v1_listener toplevel_list_impl = {
    .toplevel = toplevel_list_handle_toplevel,
    .finished = toplevel_list_handle_finished,
};

// zcosmic_toplevel_handle_v1: only workspace membership is used here. Since
// toplevel_info_manager_ is bound at version >= 2, app_id/title/closed/done all
// come from the paired ext_foreign_toplevel_handle_v1 instead (see toplevel_handle_impl).
static void cosmic_toplevel_handle_closed(void*, zcosmic_toplevel_handle_v1*) {}
static void cosmic_toplevel_handle_done(void*, zcosmic_toplevel_handle_v1*) {}
static void cosmic_toplevel_handle_title(void*, zcosmic_toplevel_handle_v1*, const char*) {}
static void cosmic_toplevel_handle_app_id(void*, zcosmic_toplevel_handle_v1*, const char*) {}
static void cosmic_toplevel_handle_output_enter(void*, zcosmic_toplevel_handle_v1*, wl_output*) {}
static void cosmic_toplevel_handle_output_leave(void*, zcosmic_toplevel_handle_v1*, wl_output*) {}
static void cosmic_toplevel_handle_workspace_enter_v1(void*, zcosmic_toplevel_handle_v1*,
                                                      zcosmic_workspace_handle_v1*) {}
static void cosmic_toplevel_handle_workspace_leave_v1(void*, zcosmic_toplevel_handle_v1*,
                                                      zcosmic_workspace_handle_v1*) {}
static void cosmic_toplevel_handle_state(void*, zcosmic_toplevel_handle_v1*, wl_array*) {}
static void cosmic_toplevel_handle_geometry(void*, zcosmic_toplevel_handle_v1*, wl_output*, int32_t,
                                            int32_t, int32_t, int32_t) {}

static void cosmic_toplevel_handle_ext_workspace_enter(void* data,
                                                       zcosmic_toplevel_handle_v1* handle,
                                                       ext_workspace_handle_v1* ws) {
  static_cast<WorkspaceThumbnails*>(data)->handle_cosmic_workspace_enter(handle, ws);
}

static void cosmic_toplevel_handle_ext_workspace_leave(void* data,
                                                       zcosmic_toplevel_handle_v1* handle,
                                                       ext_workspace_handle_v1* ws) {
  static_cast<WorkspaceThumbnails*>(data)->handle_cosmic_workspace_leave(handle, ws);
}

static const struct zcosmic_toplevel_handle_v1_listener cosmic_toplevel_handle_impl = {
    .closed = cosmic_toplevel_handle_closed,
    .done = cosmic_toplevel_handle_done,
    .title = cosmic_toplevel_handle_title,
    .app_id = cosmic_toplevel_handle_app_id,
    .output_enter = cosmic_toplevel_handle_output_enter,
    .output_leave = cosmic_toplevel_handle_output_leave,
    .workspace_enter = cosmic_toplevel_handle_workspace_enter_v1,
    .workspace_leave = cosmic_toplevel_handle_workspace_leave_v1,
    .state = cosmic_toplevel_handle_state,
    .geometry = cosmic_toplevel_handle_geometry,
    .ext_workspace_enter = cosmic_toplevel_handle_ext_workspace_enter,
    .ext_workspace_leave = cosmic_toplevel_handle_ext_workspace_leave,
};

/* ---- Overlap probe: invisible full-output surface used to discover every
 * other layer-surface's reserved (exclusive) area, via cosmic-overlap-notify */

static void probe_handle_configure(void* data, zwlr_layer_surface_v1*, uint32_t serial,
                                   uint32_t width, uint32_t height) {
  static_cast<WorkspaceThumbnails*>(data)->handle_probe_configure(serial, width, height);
}

static void probe_handle_closed(void* data, zwlr_layer_surface_v1*) {
  static_cast<WorkspaceThumbnails*>(data)->handle_probe_closed();
}

static const struct zwlr_layer_surface_v1_listener probe_layer_surface_impl = {
    .configure = probe_handle_configure,
    .closed = probe_handle_closed,
};

static void probe_handle_toplevel_enter(void*, zcosmic_overlap_notification_v1*,
                                        ext_foreign_toplevel_handle_v1*, int32_t, int32_t, int32_t,
                                        int32_t) {}

static void probe_handle_toplevel_leave(void*, zcosmic_overlap_notification_v1*,
                                        ext_foreign_toplevel_handle_v1*) {}

static void probe_handle_layer_enter(void* data, zcosmic_overlap_notification_v1*,
                                     const char* identifier, const char* ns, uint32_t exclusive,
                                     uint32_t /*layer*/, int32_t x, int32_t y, int32_t width,
                                     int32_t height) {
  static_cast<WorkspaceThumbnails*>(data)->handle_layer_enter(identifier, ns, exclusive, x, y,
                                                              width, height);
}

static void probe_handle_layer_leave(void* data, zcosmic_overlap_notification_v1*,
                                     const char* identifier) {
  static_cast<WorkspaceThumbnails*>(data)->handle_layer_leave(identifier);
}

static const struct zcosmic_overlap_notification_v1_listener probe_notification_impl = {
    .toplevel_enter = probe_handle_toplevel_enter,
    .toplevel_leave = probe_handle_toplevel_leave,
    .layer_enter = probe_handle_layer_enter,
    .layer_leave = probe_handle_layer_leave,
};

// A fully-transparent shm buffer sized to (width, height). Must match the
// surface's real configured size: cosmic-comp appears to clip reported overlap
// rects to the requesting surface's *attached buffer* extent, not just its
// anchor-implied logical size, so a placeholder 1x1 buffer here would make
// every reported overlap rect degenerate to 1x1 too.
static wl_buffer* create_transparent_buffer(wl_shm* shm, uint32_t width, uint32_t height) {
  if (!shm || width == 0 || height == 0) return nullptr;
  const size_t stride = static_cast<size_t>(width) * 4;
  const size_t size = stride * height;
  int fd = memfd_create("waybar-cosmic-probe-buffer", MFD_CLOEXEC);
  if (fd < 0) {
    spdlog::error("[cosmic/workspaces]: memfd_create failed: {}", strerror(errno));
    return nullptr;
  }
  if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
    spdlog::error("[cosmic/workspaces]: ftruncate failed: {}", strerror(errno));
    close(fd);
    return nullptr;
  }
  void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    spdlog::error("[cosmic/workspaces]: mmap failed: {}", strerror(errno));
    close(fd);
    return nullptr;
  }
  std::memset(data, 0, size);
  munmap(data, size);

  wl_shm_pool* pool = wl_shm_create_pool(shm, fd, static_cast<int32_t>(size));
  close(fd);
  wl_buffer* buffer =
      wl_shm_pool_create_buffer(pool, 0, static_cast<int32_t>(width), static_cast<int32_t>(height),
                                static_cast<int32_t>(stride), WL_SHM_FORMAT_ARGB8888);
  wl_shm_pool_destroy(pool);
  return buffer;
}

WorkspaceThumbnails::WorkspaceThumbnails(const std::string& id, const waybar::Bar& bar,
                                         const Json::Value& config)
    : waybar::AModule(config, "cosmic-workspaces", id, false, false),
      bar_(bar),
      capture_slots_(config["max-concurrent-captures"].isUInt()
                         ? std::max(1, static_cast<int>(config["max-concurrent-captures"].asUInt()))
                         : 2) {
  box_.set_name("cosmic-workspaces");
  if (!id.empty()) {
    box_.get_style_context()->add_class(id);
  }
  box_.get_style_context()->add_class(MODULE_CLASS);
  // The bar's own cross-axis thickness (height for a horizontal bar, width for
  // a vertical one) can leave the grid smaller than its allocation - e.g. once
  // workspace-wrap-size shrinks thumbnails - so centre it on both axes rather
  // than hugging the container's default (start) edge.
  box_.set_halign(Gtk::ALIGN_CENTER);
  box_.set_valign(Gtk::ALIGN_CENTER);
  event_box_.add(box_);

  if (config_["all-outputs"].isBool()) {
    all_outputs_ = config_["all-outputs"].asBool();
  }
  if (config_["interval"].isUInt()) {
    interval_ = std::chrono::milliseconds(config_["interval"].asUInt());
  }
  if (config_["thumbnail-width"].isUInt()) {
    thumb_width_ = config_["thumbnail-width"].asUInt();
  }
  if (config_["thumbnail-height"].isUInt()) {
    thumb_height_ = config_["thumbnail-height"].asUInt();
  }
  if (config_["thumbnail-min-width"].isUInt()) {
    thumb_min_width_ = config_["thumbnail-min-width"].asUInt();
  }
  if (config_["thumbnail-min-height"].isUInt()) {
    thumb_min_height_ = config_["thumbnail-min-height"].asUInt();
  }
  if (config_["workspace-wrap-size"].isUInt()) {
    workspace_wrap_size_ = config_["workspace-wrap-size"].asUInt();
  }

  if (config_["display"].isString()) {
    const auto& v = config_["display"].asString();
    if (v == "live") {
      options_.display = DisplayMode::Live;
    } else if (v == "standard") {
      options_.display = DisplayMode::Standard;
    } else {
      spdlog::warn("[cosmic/workspaces]: unknown display mode '{}', expected live/standard", v);
    }
  }
  if (config_["crop-dead-zones"].isBool()) {
    options_.crop_dead_zones =
        config_["crop-dead-zones"].asBool() ? CropMode::OwnBar : CropMode::None;
  } else if (config_["crop-dead-zones"].isString()) {
    const auto& v = config_["crop-dead-zones"].asString();
    if (v == "own-bar") {
      options_.crop_dead_zones = CropMode::OwnBar;
    } else if (v == "all-bars") {
      options_.crop_dead_zones = CropMode::AllBars;
    } else if (v == "none") {
      options_.crop_dead_zones = CropMode::None;
    } else {
      spdlog::warn(
          "[cosmic/workspaces]: unknown crop-dead-zones '{}', expected "
          "own-bar/all-bars/none",
          v);
    }
  }
  if (config_["highlight-show"].isBool()) {
    options_.highlight_show = config_["highlight-show"].asBool();
  }
  if (config_["highlight-line-size"].isUInt()) {
    options_.highlight_line_size = config_["highlight-line-size"].asUInt();
  }
  if (config_["mouse-scroll-enabled"].isBool()) {
    options_.mouse_scroll_enabled = config_["mouse-scroll-enabled"].asBool();
  }
  if (config_["thumbnail-label-show"].isBool()) {
    options_.thumbnail_label_show = config_["thumbnail-label-show"].asBool();
  }
  if (config_["thumbnail-label"].isString()) {
    options_.thumbnail_label = config_["thumbnail-label"].asString();
  }
  options_.thumbnail_label_halign =
      parse_align(config_["thumbnail-label-horizontal-align"], Gtk::ALIGN_CENTER);
  options_.thumbnail_label_valign =
      parse_align(config_["thumbnail-label-vertical-align"], Gtk::ALIGN_CENTER);
  if (config_["label-show"].isBool()) {
    options_.label_show = config_["label-show"].asBool();
  }
  if (config_["label-position"].isString()) {
    const auto& v = config_["label-position"].asString();
    if (v == "before") {
      options_.label_position = LabelPosition::Before;
    } else if (v == "after") {
      options_.label_position = LabelPosition::After;
    } else {
      spdlog::warn("[cosmic/workspaces]: unknown label-position '{}', expected before/after", v);
    }
  }
  if (config_["spacing"].isUInt()) {
    options_.spacing = config_["spacing"].asUInt();
  }
  box_.set_row_spacing(options_.spacing);
  box_.set_column_spacing(options_.spacing);

  if (config_["icons-show"].isBool()) {
    options_.icons_show = config_["icons-show"].asBool();
  }
  if (config_["icons-orientation"].isString()) {
    const auto& v = config_["icons-orientation"].asString();
    if (v == "horizontal") {
      options_.icons_orientation = IconsOrientation::Horizontal;
    } else if (v == "vertical") {
      options_.icons_orientation = IconsOrientation::Vertical;
    } else {
      spdlog::warn("[cosmic/workspaces]: unknown icons-orientation '{}', expected "
                   "horizontal/vertical",
                   v);
    }
  }
  if (config_["icons-position"].isString()) {
    const auto& v = config_["icons-position"].asString();
    if (v == "embedded") {
      options_.icons_position = IconsPosition::Embedded;
    } else if (v == "beforethumbnail") {
      options_.icons_position = IconsPosition::BeforeThumbnail;
    } else if (v == "afterthumbnail") {
      options_.icons_position = IconsPosition::AfterThumbnail;
    } else if (v == "beforelabel") {
      options_.icons_position = IconsPosition::BeforeLabel;
    } else if (v == "afterlabel") {
      options_.icons_position = IconsPosition::AfterLabel;
    } else {
      spdlog::warn(
          "[cosmic/workspaces]: unknown icons-position '{}', expected embedded/beforethumbnail/"
          "afterthumbnail/beforelabel/afterlabel",
          v);
    }
  }
  if (config_["icons-size"].isUInt()) {
    options_.icons_size = config_["icons-size"].asUInt();
  }
  if (config_["icons-spacing"].isUInt()) {
    options_.icons_spacing = config_["icons-spacing"].asUInt();
  }
  if (config_["icons-edge"].isString()) {
    const auto& v = config_["icons-edge"].asString();
    if (v == "top") {
      options_.icons_edge = IconsEdge::Top;
    } else if (v == "bottom") {
      options_.icons_edge = IconsEdge::Bottom;
    } else if (v == "left") {
      options_.icons_edge = IconsEdge::Left;
    } else if (v == "right") {
      options_.icons_edge = IconsEdge::Right;
    } else {
      spdlog::warn("[cosmic/workspaces]: unknown icons-edge '{}', expected "
                   "top/bottom/left/right",
                   v);
    }
  }
  if (config_["icons-align"].isString()) {
    const auto& v = config_["icons-align"].asString();
    if (v == "start") {
      options_.icons_align = IconsAlign::Start;
    } else if (v == "center") {
      options_.icons_align = IconsAlign::Center;
    } else if (v == "end") {
      options_.icons_align = IconsAlign::End;
    } else {
      spdlog::warn("[cosmic/workspaces]: unknown icons-align '{}', expected start/center/end", v);
    }
  }

  if (options_.mouse_scroll_enabled) {
    event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK);
    event_box_.signal_scroll_event().connect(sigc::mem_fun(*this, &WorkspaceThumbnails::on_scroll));
  }

  wl_display* display = Client::inst()->wl_display;
  wl_registry* registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener_impl, this);
  wl_display_roundtrip(display);

  // The registry enumerates globals in compositor-defined order, so any ext_foreign_toplevel_v1
  // "toplevel" events may have already arrived (and been handled) before toplevel_info_manager_
  // was bound within the same roundtrip; retroactively fetch their cosmic wrapper now.
  if (toplevel_info_manager_ && options_.icons_show) {
    for (auto& [foreign, toplevel] : toplevels_) {
      if (toplevel.cosmic) continue;
      toplevel.cosmic = zcosmic_toplevel_info_v1_get_cosmic_toplevel(toplevel_info_manager_, foreign);
      if (toplevel.cosmic) {
        zcosmic_toplevel_handle_v1_add_listener(toplevel.cosmic, &cosmic_toplevel_handle_impl,
                                                this);
        cosmic_to_foreign_[toplevel.cosmic] = foreign;
      }
    }
    wl_display_roundtrip(display);
  }

  if (!workspace_manager_) {
    spdlog::error("[cosmic/workspaces]: Compositor does not support ext-workspace-v1");
  }
  if (options_.display == DisplayMode::Live && (!capture_manager_ || !source_manager_)) {
    spdlog::error(
        "[cosmic/workspaces]: Compositor does not support the COSMIC workspace screencopy "
        "protocol; thumbnails will stay blank");
  }
  if (options_.icons_show && (!foreign_toplevel_list_ || !toplevel_info_manager_)) {
    spdlog::warn(
        "[cosmic/workspaces]: Compositor does not support ext-foreign-toplevel-list-v1 / "
        "cosmic-toplevel-info; icons-show will have no effect");
  }

  if (options_.display == DisplayMode::Live && options_.crop_dead_zones != CropMode::None) {
    start_overlap_probe();
  }
}

WorkspaceThumbnails::~WorkspaceThumbnails() {
  pending_active_timeout_.disconnect();
  thumbnails_.clear();
  stop_overlap_probe();

  for (auto& group : groups_) {
    ext_workspace_group_handle_v1_destroy(group.handle);
  }
  groups_.clear();

  for (auto* handle : workspaces_) {
    ext_workspace_handle_v1_destroy(handle);
  }
  workspaces_.clear();

  if (workspace_manager_) {
    wl_display* display = Client::inst()->wl_display;
    ext_workspace_manager_v1_stop(workspace_manager_);
    wl_display_roundtrip(display);
    if (workspace_manager_) {
      spdlog::warn("[cosmic/workspaces]: Workspace manager destroyed before .finished event");
      ext_workspace_manager_v1_destroy(workspace_manager_);
      workspace_manager_ = nullptr;
    }
  }

  if (capture_manager_) {
    ext_image_copy_capture_manager_v1_destroy(capture_manager_);
    capture_manager_ = nullptr;
  }
  if (source_manager_) {
    zcosmic_workspace_image_capture_source_manager_v1_destroy(source_manager_);
    source_manager_ = nullptr;
  }
  if (overlap_notify_) {
    zcosmic_overlap_notify_v1_destroy(overlap_notify_);
    overlap_notify_ = nullptr;
  }
  if (layer_shell_) {
    zwlr_layer_shell_v1_destroy(layer_shell_);
    layer_shell_ = nullptr;
  }
  if (compositor_) {
    wl_compositor_destroy(compositor_);
    compositor_ = nullptr;
  }
  if (shm_) {
    wl_shm_destroy(shm_);
    shm_ = nullptr;
  }

  for (auto& [foreign, toplevel] : toplevels_) {
    if (toplevel.cosmic) zcosmic_toplevel_handle_v1_destroy(toplevel.cosmic);
    ext_foreign_toplevel_handle_v1_destroy(foreign);
  }
  toplevels_.clear();
  cosmic_to_foreign_.clear();

  if (foreign_toplevel_list_) {
    ext_foreign_toplevel_list_v1_destroy(foreign_toplevel_list_);
    foreign_toplevel_list_ = nullptr;
  }
  if (toplevel_manager_) {
    zcosmic_toplevel_manager_v1_destroy(toplevel_manager_);
    toplevel_manager_ = nullptr;
  }
  // zcosmic_toplevel_info_v1 (version >= 2) has no destroy request; it's expected
  // to simply be dropped, mirroring the compositor-driven `finished` teardown.
  toplevel_info_manager_ = nullptr;
  if (seat_) {
    wl_seat_release(seat_);
    seat_ = nullptr;
  }
}

void WorkspaceThumbnails::register_manager(wl_registry* registry, uint32_t name,
                                           const char* interface, uint32_t version) {
  if (std::strcmp(interface, wl_shm_interface.name) == 0) {
    if (shm_) return;
    shm_ = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface, ext_workspace_manager_v1_interface.name) == 0) {
    if (workspace_manager_) return;
    version = std::min<uint32_t>(version, ext_workspace_manager_v1_interface.version);
    workspace_manager_ = static_cast<ext_workspace_manager_v1*>(
        wl_registry_bind(registry, name, &ext_workspace_manager_v1_interface, version));
    ext_workspace_manager_v1_add_listener(workspace_manager_, &workspace_manager_impl, this);
  } else if (std::strcmp(interface, ext_image_copy_capture_manager_v1_interface.name) == 0) {
    if (capture_manager_) return;
    version = std::min<uint32_t>(version, ext_image_copy_capture_manager_v1_interface.version);
    capture_manager_ = static_cast<ext_image_copy_capture_manager_v1*>(
        wl_registry_bind(registry, name, &ext_image_copy_capture_manager_v1_interface, version));
  } else if (std::strcmp(interface,
                         zcosmic_workspace_image_capture_source_manager_v1_interface.name) == 0) {
    if (source_manager_) return;
    version = std::min<uint32_t>(
        version, zcosmic_workspace_image_capture_source_manager_v1_interface.version);
    source_manager_ =
        static_cast<zcosmic_workspace_image_capture_source_manager_v1*>(wl_registry_bind(
            registry, name, &zcosmic_workspace_image_capture_source_manager_v1_interface, version));
  } else if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
    if (compositor_) return;
    version = std::min<uint32_t>(version, wl_compositor_interface.version);
    compositor_ = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, version));
  } else if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
    if (layer_shell_) return;
    version = std::min<uint32_t>(version, zwlr_layer_shell_v1_interface.version);
    layer_shell_ = static_cast<zwlr_layer_shell_v1*>(
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version));
  } else if (std::strcmp(interface, zcosmic_overlap_notify_v1_interface.name) == 0) {
    if (overlap_notify_) return;
    version = std::min<uint32_t>(version, zcosmic_overlap_notify_v1_interface.version);
    overlap_notify_ = static_cast<zcosmic_overlap_notify_v1*>(
        wl_registry_bind(registry, name, &zcosmic_overlap_notify_v1_interface, version));
  } else if (std::strcmp(interface, ext_foreign_toplevel_list_v1_interface.name) == 0) {
    if (foreign_toplevel_list_) return;
    version = std::min<uint32_t>(version, ext_foreign_toplevel_list_v1_interface.version);
    foreign_toplevel_list_ = static_cast<ext_foreign_toplevel_list_v1*>(
        wl_registry_bind(registry, name, &ext_foreign_toplevel_list_v1_interface, version));
    ext_foreign_toplevel_list_v1_add_listener(foreign_toplevel_list_, &toplevel_list_impl, this);
  } else if (std::strcmp(interface, zcosmic_toplevel_info_v1_interface.name) == 0) {
    if (toplevel_info_manager_) return;
    // Bind version >= 2 so we use get_cosmic_toplevel() + ext_foreign_toplevel_handle_v1
    // for data, rather than the deprecated v1 toplevel/app_id/title events.
    version = std::max<uint32_t>(2, std::min<uint32_t>(version, zcosmic_toplevel_info_v1_interface.version));
    toplevel_info_manager_ = static_cast<zcosmic_toplevel_info_v1*>(
        wl_registry_bind(registry, name, &zcosmic_toplevel_info_v1_interface, version));
  } else if (std::strcmp(interface, zcosmic_toplevel_manager_v1_interface.name) == 0) {
    if (toplevel_manager_) return;
    version = std::min<uint32_t>(version, zcosmic_toplevel_manager_v1_interface.version);
    toplevel_manager_ = static_cast<zcosmic_toplevel_manager_v1*>(
        wl_registry_bind(registry, name, &zcosmic_toplevel_manager_v1_interface, version));
  } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
    if (seat_) return;
    version = std::min<uint32_t>(version, wl_seat_interface.version);
    seat_ = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, version));
  }
}

void WorkspaceThumbnails::handle_workspace_group_create(ext_workspace_group_handle_v1* handle) {
  ext_workspace_group_handle_v1_add_listener(handle, &workspace_group_impl, this);
  groups_.push_back(GroupInfo{handle, {}, {}});
}

void WorkspaceThumbnails::handle_workspace_create(ext_workspace_handle_v1* handle) {
  ext_workspace_handle_v1_add_listener(handle, &workspace_handle_impl, this);
  workspaces_.push_back(handle);
}

void WorkspaceThumbnails::handle_manager_done() {
  needs_sync_ = true;
  dp.emit();
}

void WorkspaceThumbnails::handle_manager_finished() {
  ext_workspace_manager_v1_destroy(workspace_manager_);
  workspace_manager_ = nullptr;
}

void WorkspaceThumbnails::handle_group_output_enter(ext_workspace_group_handle_v1* group,
                                                    wl_output* output) {
  const auto it =
      std::find_if(groups_.begin(), groups_.end(), [group](auto& g) { return g.handle == group; });
  if (it != groups_.end()) it->outputs.push_back(output);
}

void WorkspaceThumbnails::handle_group_output_leave(ext_workspace_group_handle_v1* group,
                                                    wl_output* output) {
  const auto it =
      std::find_if(groups_.begin(), groups_.end(), [group](auto& g) { return g.handle == group; });
  if (it != groups_.end()) {
    it->outputs.erase(std::remove(it->outputs.begin(), it->outputs.end(), output),
                      it->outputs.end());
  }
}

void WorkspaceThumbnails::handle_group_workspace_enter(ext_workspace_group_handle_v1* group,
                                                       ext_workspace_handle_v1* ws) {
  const auto it =
      std::find_if(groups_.begin(), groups_.end(), [group](auto& g) { return g.handle == group; });
  if (it != groups_.end()) it->workspaces.push_back(ws);
}

void WorkspaceThumbnails::handle_group_workspace_leave(ext_workspace_group_handle_v1* group,
                                                       ext_workspace_handle_v1* ws) {
  const auto it =
      std::find_if(groups_.begin(), groups_.end(), [group](auto& g) { return g.handle == group; });
  if (it != groups_.end()) {
    it->workspaces.erase(std::remove(it->workspaces.begin(), it->workspaces.end(), ws),
                         it->workspaces.end());
  }
}

void WorkspaceThumbnails::handle_group_removed(ext_workspace_group_handle_v1* group) {
  const auto it =
      std::find_if(groups_.begin(), groups_.end(), [group](auto& g) { return g.handle == group; });
  if (it != groups_.end()) {
    ext_workspace_group_handle_v1_destroy(it->handle);
    groups_.erase(it);
  }
}

void WorkspaceThumbnails::handle_workspace_removed(ext_workspace_handle_v1* handle) {
  thumbnails_.erase(handle);
  meta_.erase(handle);
  if (pending_active_ == handle) {
    pending_active_ = nullptr;
    pending_active_timeout_.disconnect();
  }
  for (auto& group : groups_) {
    group.workspaces.erase(std::remove(group.workspaces.begin(), group.workspaces.end(), handle),
                           group.workspaces.end());
  }
  for (auto& [foreign, toplevel] : toplevels_) {
    auto& workspaces = toplevel.workspaces;
    workspaces.erase(std::remove(workspaces.begin(), workspaces.end(), handle), workspaces.end());
  }
  const auto it = std::find(workspaces_.begin(), workspaces_.end(), handle);
  if (it != workspaces_.end()) {
    ext_workspace_handle_v1_destroy(*it);
    workspaces_.erase(it);
  }
  // Remaining thumbnails' displayed numbers shift once a preceding workspace is gone.
  for (auto& [remaining_handle, thumb] : thumbnails_) {
    thumb->refresh_style();
  }
}

void WorkspaceThumbnails::handle_workspace_name(ext_workspace_handle_v1* handle,
                                                const std::string& name) {
  meta_[handle].name = name;
  refresh_thumbnail(handle);
}

void WorkspaceThumbnails::handle_workspace_state(ext_workspace_handle_v1* handle, uint32_t state) {
  meta_[handle].active = (state & EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE) != 0;

  if (meta_[handle].active && handle == pending_active_) {
    // Real confirmation of our own pending guess arrived; stop overriding.
    // A late/stale confirmation for a *superseded* request (some other handle)
    // must NOT clear a newer pending_active_ - only an exact match does.
    pending_active_ = nullptr;
    pending_active_timeout_.disconnect();
    for (auto& [unused, thumb] : thumbnails_) {
      thumb->refresh_style();
    }
    return;
  }

  refresh_thumbnail(handle);
}

void WorkspaceThumbnails::refresh_thumbnail(ext_workspace_handle_v1* handle) {
  const auto it = thumbnails_.find(handle);
  if (it != thumbnails_.end()) {
    it->second->refresh_style();
  }
}

bool WorkspaceThumbnails::is_effectively_active(ext_workspace_handle_v1* handle) const {
  if (pending_active_) {
    return handle == pending_active_;
  }
  const auto* meta = meta_for(handle);
  return meta && meta->active;
}

void WorkspaceThumbnails::request_activate(ext_workspace_handle_v1* handle) {
  ext_workspace_handle_v1_activate(handle);
  commit();

  pending_active_ = handle;
  pending_active_timeout_.disconnect();
  pending_active_timeout_ = Glib::signal_timeout().connect(
      [this]() {
        pending_active_ = nullptr;
        for (auto& [unused, thumb] : thumbnails_) {
          thumb->refresh_style();
        }
        return false;
      },
      1500);

  for (auto& [unused, thumb] : thumbnails_) {
    thumb->refresh_style();
  }
}

const WorkspaceThumbnails::WorkspaceMeta* WorkspaceThumbnails::meta_for(
    ext_workspace_handle_v1* handle) const {
  const auto it = meta_.find(handle);
  return it != meta_.end() ? &it->second : nullptr;
}

int WorkspaceThumbnails::number_for(ext_workspace_handle_v1* handle) const {
  // Number among the workspaces actually shown on this bar, not the compositor's
  // full (possibly other-output) list, so numbering has no gaps with all-outputs: false.
  const auto visible = visible_workspaces();
  const auto it = std::find(visible.begin(), visible.end(), handle);
  if (it == visible.end()) return 0;
  return static_cast<int>(std::distance(visible.begin(), it)) + 1;
}

CropRect WorkspaceThumbnails::dead_zone_crop(ext_workspace_handle_v1* handle, double width,
                                             double height) const {
  const CropRect full{0, 0, width, height};
  if (options_.crop_dead_zones == CropMode::None) return full;
  if (!bar_.output || bar_.output->width <= 0 || bar_.output->height <= 0) return full;

  // Reserved-space geometry (compute_edge_clips) is only probed for this bar's own
  // output, so a workspace living on another output (all-outputs mode) can't be
  // cropped correctly here; leave it uncropped rather than apply the wrong geometry.
  const auto* bar_wl_output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
  const bool on_bar_output = std::any_of(groups_.begin(), groups_.end(), [&](const auto& group) {
    const bool ws_in_group = std::find(group.workspaces.begin(), group.workspaces.end(),
                                       handle) != group.workspaces.end();
    const bool group_on_bar_output = std::find(group.outputs.begin(), group.outputs.end(),
                                               bar_wl_output) != group.outputs.end();
    return ws_in_group && group_on_bar_output;
  });
  if (!on_bar_output) return full;

  const EdgeClips clips = compute_edge_clips();
  const double out_w = bar_.output->width;
  const double out_h = bar_.output->height;

  const double left = clips.left / out_w * width;
  const double right = clips.right / out_w * width;
  const double top = clips.top / out_h * height;
  const double bottom = clips.bottom / out_h * height;

  const double x = std::min(left, width);
  const double y = std::min(top, height);
  const double w = std::max(0.0, width - left - right);
  const double h = std::max(0.0, height - top - bottom);
  return {x, y, w, h};
}

WorkspaceThumbnails::EdgeClips WorkspaceThumbnails::compute_edge_clips() const {
  EdgeClips clips{};
  if (options_.crop_dead_zones == CropMode::None) return clips;
  if (!bar_.output || bar_.output->width <= 0 || bar_.output->height <= 0) return clips;

  const double w = bar_.output->width;
  const double h = bar_.output->height;
  const std::string own_ns =
      bar_.config["name"].isString() ? bar_.config["name"].asString() : "waybar";

  for (const auto& [id, r] : reserved_) {
    if (r.exclusive == 0) continue;
    if (options_.crop_dead_zones == CropMode::OwnBar && r.ns != own_ns) continue;

    if (r.width >= r.height) {
      if (r.y + r.height / 2.0 < h / 2.0) {
        clips.top = std::max(clips.top, static_cast<uint32_t>(std::lround(r.y + r.height)));
      } else {
        clips.bottom = std::max(clips.bottom, static_cast<uint32_t>(std::lround(h - r.y)));
      }
    } else if (r.x + r.width / 2.0 < w / 2.0) {
      clips.left = std::max(clips.left, static_cast<uint32_t>(std::lround(r.x + r.width)));
    } else {
      clips.right = std::max(clips.right, static_cast<uint32_t>(std::lround(w - r.x)));
    }
  }

  if (!logged_clips_once_ || !(clips == last_logged_clips_)) {
    spdlog::debug(
        "[cosmic/workspaces]: edge clips changed: top={} bottom={} left={} right={} "
        "(output {}x{}, {} reserved rects, mode={})",
        clips.top, clips.bottom, clips.left, clips.right, w, h, reserved_.size(),
        options_.crop_dead_zones == CropMode::AllBars ? "all-bars" : "own-bar");
    last_logged_clips_ = clips;
    logged_clips_once_ = true;
  }
  return clips;
}

void WorkspaceThumbnails::start_overlap_probe() {
  if (!compositor_ || !layer_shell_ || !overlap_notify_ || !shm_) {
    spdlog::warn(
        "[cosmic/workspaces]: Missing wl_compositor/zwlr_layer_shell_v1/"
        "zcosmic_overlap_notify_v1; crop-dead-zones will have no effect");
    return;
  }

  probe_surface_ = wl_compositor_create_surface(compositor_);
  wl_region* empty_region = wl_compositor_create_region(compositor_);
  wl_surface_set_input_region(probe_surface_, empty_region);
  wl_region_destroy(empty_region);

  auto* output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
  probe_layer_surface_ = zwlr_layer_shell_v1_get_layer_surface(layer_shell_, probe_surface_, output,
                                                               ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
                                                               "waybar-cosmic-workspaces-probe");
  zwlr_layer_surface_v1_add_listener(probe_layer_surface_, &probe_layer_surface_impl, this);
  zwlr_layer_surface_v1_set_anchor(probe_layer_surface_, ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
                                                             ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                                                             ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
                                                             ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
  zwlr_layer_surface_v1_set_exclusive_zone(probe_layer_surface_, -1);
  zwlr_layer_surface_v1_set_keyboard_interactivity(
      probe_layer_surface_, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
  wl_surface_commit(probe_surface_);
}

void WorkspaceThumbnails::stop_overlap_probe() {
  reserved_.clear();
  if (probe_notification_) {
    zcosmic_overlap_notification_v1_destroy(probe_notification_);
    probe_notification_ = nullptr;
  }
  if (probe_layer_surface_) {
    zwlr_layer_surface_v1_destroy(probe_layer_surface_);
    probe_layer_surface_ = nullptr;
  }
  if (probe_surface_) {
    wl_surface_destroy(probe_surface_);
    probe_surface_ = nullptr;
  }
  if (probe_buffer_) {
    wl_buffer_destroy(probe_buffer_);
    probe_buffer_ = nullptr;
  }
  probe_buffer_width_ = 0;
  probe_buffer_height_ = 0;
}

void WorkspaceThumbnails::handle_probe_configure(uint32_t serial, uint32_t width, uint32_t height) {
  spdlog::debug("[cosmic/workspaces]: probe configure serial={} size={}x{}", serial, width, height);
  zwlr_layer_surface_v1_ack_configure(probe_layer_surface_, serial);

  if (width > 0 && height > 0 &&
      (!probe_buffer_ || probe_buffer_width_ != width || probe_buffer_height_ != height)) {
    if (probe_buffer_) {
      wl_buffer_destroy(probe_buffer_);
    }
    probe_buffer_ = create_transparent_buffer(shm_, width, height);
    probe_buffer_width_ = probe_buffer_ ? width : 0;
    probe_buffer_height_ = probe_buffer_ ? height : 0;
  }
  if (probe_buffer_) {
    wl_surface_attach(probe_surface_, probe_buffer_, 0, 0);
    wl_surface_damage_buffer(probe_surface_, 0, 0, static_cast<int32_t>(probe_buffer_width_),
                             static_cast<int32_t>(probe_buffer_height_));
  }
  wl_surface_commit(probe_surface_);

  if (!probe_notification_) {
    probe_notification_ =
        zcosmic_overlap_notify_v1_notify_on_overlap(overlap_notify_, probe_layer_surface_);
    zcosmic_overlap_notification_v1_add_listener(probe_notification_, &probe_notification_impl,
                                                 this);
  }
}

void WorkspaceThumbnails::handle_probe_closed() {
  spdlog::warn(
      "[cosmic/workspaces]: overlap probe surface closed by compositor; crop-dead-zones "
      "will stop updating until this module is recreated");
  stop_overlap_probe();
}

void WorkspaceThumbnails::handle_layer_enter(const std::string& identifier, const std::string& ns,
                                             uint32_t exclusive, int32_t x, int32_t y,
                                             int32_t width, int32_t height) {
  spdlog::debug("[cosmic/workspaces]: layer_enter id={} ns={} exclusive={} rect=({},{} {}x{})",
                identifier, ns, exclusive, x, y, width, height);
  reserved_[identifier] = ReservedRect{ns, exclusive, x, y, width, height};
  for (auto& [handle, thumb] : thumbnails_) {
    thumb->widget().queue_draw();
  }
}

void WorkspaceThumbnails::handle_layer_leave(const std::string& identifier) {
  const auto it = reserved_.find(identifier);
  spdlog::debug("[cosmic/workspaces]: layer_leave id={} ns={}", identifier,
                it != reserved_.end() ? it->second.ns : std::string("?"));
  reserved_.erase(identifier);
  for (auto& [handle, thumb] : thumbnails_) {
    thumb->widget().queue_draw();
  }
}

void WorkspaceThumbnails::handle_toplevel_list_toplevel(ext_foreign_toplevel_handle_v1* handle) {
  ext_foreign_toplevel_handle_v1_add_listener(handle, &toplevel_handle_impl, this);

  Toplevel toplevel;
  toplevel.foreign = handle;
  if (toplevel_info_manager_) {
    toplevel.cosmic =
        zcosmic_toplevel_info_v1_get_cosmic_toplevel(toplevel_info_manager_, handle);
    if (toplevel.cosmic) {
      zcosmic_toplevel_handle_v1_add_listener(toplevel.cosmic, &cosmic_toplevel_handle_impl, this);
      cosmic_to_foreign_[toplevel.cosmic] = handle;
    }
  }
  toplevels_[handle] = std::move(toplevel);
}

void WorkspaceThumbnails::handle_toplevel_list_finished() {
  if (foreign_toplevel_list_) {
    ext_foreign_toplevel_list_v1_destroy(foreign_toplevel_list_);
    foreign_toplevel_list_ = nullptr;
  }
}

void WorkspaceThumbnails::handle_toplevel_closed(ext_foreign_toplevel_handle_v1* handle) {
  const auto it = toplevels_.find(handle);
  if (it == toplevels_.end()) return;

  const auto affected = it->second.workspaces;
  if (it->second.cosmic) {
    cosmic_to_foreign_.erase(it->second.cosmic);
    zcosmic_toplevel_handle_v1_destroy(it->second.cosmic);
  }
  toplevels_.erase(it);
  ext_foreign_toplevel_handle_v1_destroy(handle);

  for (auto* ws : affected) {
    refresh_icons(ws);
  }
}

void WorkspaceThumbnails::handle_toplevel_app_id(ext_foreign_toplevel_handle_v1* handle,
                                                 const std::string& app_id) {
  const auto it = toplevels_.find(handle);
  if (it == toplevels_.end()) return;
  it->second.app_id = app_id;
}

void WorkspaceThumbnails::handle_toplevel_done(ext_foreign_toplevel_handle_v1* handle) {
  const auto it = toplevels_.find(handle);
  if (it == toplevels_.end()) return;
  for (auto* ws : it->second.workspaces) {
    refresh_icons(ws);
  }
}

void WorkspaceThumbnails::handle_cosmic_workspace_enter(zcosmic_toplevel_handle_v1* handle,
                                                        ext_workspace_handle_v1* ws) {
  const auto fit = cosmic_to_foreign_.find(handle);
  if (fit == cosmic_to_foreign_.end()) return;
  const auto it = toplevels_.find(fit->second);
  if (it == toplevels_.end()) return;
  auto& workspaces = it->second.workspaces;
  if (std::find(workspaces.begin(), workspaces.end(), ws) == workspaces.end()) {
    workspaces.push_back(ws);
  }
  refresh_icons(ws);
}

void WorkspaceThumbnails::handle_cosmic_workspace_leave(zcosmic_toplevel_handle_v1* handle,
                                                        ext_workspace_handle_v1* ws) {
  const auto fit = cosmic_to_foreign_.find(handle);
  if (fit == cosmic_to_foreign_.end()) return;
  const auto it = toplevels_.find(fit->second);
  if (it == toplevels_.end()) return;
  auto& workspaces = it->second.workspaces;
  workspaces.erase(std::remove(workspaces.begin(), workspaces.end(), ws), workspaces.end());
  refresh_icons(ws);
}

std::vector<const WorkspaceThumbnails::Toplevel*> WorkspaceThumbnails::icons_for(
    ext_workspace_handle_v1* handle) const {
  std::vector<const Toplevel*> result;
  for (const auto& [foreign, toplevel] : toplevels_) {
    const auto& workspaces = toplevel.workspaces;
    if (std::find(workspaces.begin(), workspaces.end(), handle) != workspaces.end()) {
      result.push_back(&toplevel);
    }
  }
  return result;
}

void WorkspaceThumbnails::activate_toplevel(zcosmic_toplevel_handle_v1* handle) const {
  if (!toplevel_manager_ || !seat_ || !handle) return;
  zcosmic_toplevel_manager_v1_activate(toplevel_manager_, handle, seat_);
  commit();
}

void WorkspaceThumbnails::refresh_icons(ext_workspace_handle_v1* handle) {
  const auto it = thumbnails_.find(handle);
  if (it != thumbnails_.end()) {
    it->second->refresh_icons();
  }
}

void WorkspaceThumbnails::commit() const {
  if (workspace_manager_) ext_workspace_manager_v1_commit(workspace_manager_);
}

void WorkspaceThumbnails::update() {
  if (needs_sync_) {
    sync_thumbnails();
    needs_sync_ = false;
  }
  AModule::update();
}

std::vector<ext_workspace_handle_v1*> WorkspaceThumbnails::visible_workspaces() const {
  std::vector<ext_workspace_handle_v1*> visible;
  const auto* output = gdk_wayland_monitor_get_wl_output(bar_.output->monitor->gobj());
  for (auto* handle : workspaces_) {
    const bool on_bar_output =
        all_outputs_ || std::any_of(groups_.begin(), groups_.end(), [&](const auto& group) {
          const bool group_on_output =
              std::find(group.outputs.begin(), group.outputs.end(), output) != group.outputs.end();
          const bool ws_in_group = std::find(group.workspaces.begin(), group.workspaces.end(),
                                             handle) != group.workspaces.end();
          return group_on_output && ws_in_group;
        });
    if (on_bar_output) visible.push_back(handle);
  }
  return visible;
}

bool WorkspaceThumbnails::on_scroll(GdkEventScroll* event) {
  const SCROLL_DIR dir = getScrollDir(event);
  if (dir == SCROLL_DIR::NONE) return false;

  const auto visible = visible_workspaces();
  if (visible.empty()) return true;

  const auto active_it = std::find_if(visible.begin(), visible.end(),
                                      [&](auto* handle) { return is_effectively_active(handle); });
  size_t idx = active_it != visible.end() ? static_cast<size_t>(active_it - visible.begin()) : 0;
  const bool forward = dir == SCROLL_DIR::DOWN || dir == SCROLL_DIR::RIGHT;
  idx = forward ? (idx + 1) % visible.size() : (idx + visible.size() - 1) % visible.size();

  request_activate(visible[idx]);
  return true;
}

WorkspaceThumbnails::WrapLayout WorkspaceThumbnails::compute_wrap_layout(size_t count) const {
  WrapLayout layout;
  if (workspace_wrap_size_ <= 0 || count == 0) {
    layout.lines = 1;
    layout.items_per_line = static_cast<int>(count);
    layout.thumb_width = thumb_width_;
    layout.thumb_height = thumb_height_;
    return layout;
  }

  // The bar's main axis is width for a horizontal bar (items run left-to-right,
  // additional lines stack as rows) and height for a vertical bar (items run
  // top-to-bottom, additional lines stack as columns).
  const bool horizontal = bar_.orientation == Gtk::ORIENTATION_HORIZONTAL;

  // Search forward one line-count at a time: thumbnails stay full-size in a
  // single line until it overflows, and each subsequent shrink (applied once
  // per added line) is kept as long as that many lines' worth of capacity -
  // at the resulting shrunk size - still fits everything. Only once the
  // last line overflows again does another line get added and the
  // thumbnails shrink further.
  for (int lines = 1;; ++lines) {
    const int thumb_w =
        lines == 1 ? thumb_width_ : std::max(thumb_min_width_, thumb_width_ / lines);
    const int thumb_h =
        lines == 1 ? thumb_height_ : std::max(thumb_min_height_, thumb_height_ / lines);
    const int main_dim = horizontal ? thumb_w : thumb_h;
    const int capacity_per_line =
        std::max(1, (workspace_wrap_size_ + options_.spacing) / (main_dim + options_.spacing));

    if (static_cast<size_t>(capacity_per_line) * static_cast<size_t>(lines) >= count) {
      layout.lines = lines;
      // Fill each line to capacity before overflowing into the next, rather
      // than spreading items evenly across all lines.
      layout.items_per_line = capacity_per_line;
      layout.thumb_width = thumb_w;
      layout.thumb_height = thumb_h;
      break;
    }
  }
  return layout;
}

void WorkspaceThumbnails::sync_thumbnails() {
  const auto visible = visible_workspaces();

  for (auto* handle : workspaces_) {
    const bool on_bar_output = std::find(visible.begin(), visible.end(), handle) != visible.end();

    const auto it = thumbnails_.find(handle);
    if (on_bar_output) {
      if (it == thumbnails_.end()) {
        thumbnails_.emplace(handle, std::make_unique<Thumbnail>(*this, handle));
      }
    } else if (it != thumbnails_.end()) {
      box_.remove(it->second->widget());
      thumbnails_.erase(it);
    }
  }

  // Drop thumbnails for workspaces that disappeared entirely.
  for (auto it = thumbnails_.begin(); it != thumbnails_.end();) {
    if (std::find(workspaces_.begin(), workspaces_.end(), it->first) == workspaces_.end()) {
      box_.remove(it->second->widget());
      it = thumbnails_.erase(it);
    } else {
      ++it;
    }
  }

  // Re-lay-out from scratch: with workspace-wrap-size, adding/removing even one
  // workspace can shift every item's grid position and shrink/grow every thumbnail.
  const WrapLayout layout = compute_wrap_layout(visible.size());
  const bool horizontal = bar_.orientation == Gtk::ORIENTATION_HORIZONTAL;
  for (size_t i = 0; i < visible.size(); ++i) {
    auto& thumb = *thumbnails_.at(visible[i]);
    if (thumb.widget().get_parent() != nullptr) {
      box_.remove(thumb.widget());
    }
    thumb.resize(layout.thumb_width, layout.thumb_height);

    const int line = static_cast<int>(i) / layout.items_per_line;
    const int in_line = static_cast<int>(i) % layout.items_per_line;
    // Horizontal bar: lines stack as rows, items within a line run left-to-right.
    // Vertical bar: lines stack as columns, items within a line run top-to-bottom.
    const int col = horizontal ? in_line : line;
    const int row = horizontal ? line : in_line;
    box_.attach(thumb.widget(), col, row, 1, 1);
    thumb.widget().show_all();
  }
}

}  // namespace waybar::modules::cosmic
