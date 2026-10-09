#pragma once

#include <cairomm/context.h>
#include <cairomm/surface.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/enums.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/grid.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/overlay.h>
#include <sigc++/connection.h>
#include <wayland-client.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <semaphore>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "AModule.hpp"
#include "bar.hpp"
#include "util/icon_loader.hpp"
// wayland-scanner only forward-declares interfaces defined by other protocols, so the
// headers that actually define ext_workspace_handle_v1_interface /
// ext_image_capture_source_v1_interface must be included first.
#include "ext-image-capture-source-v1-client-protocol.h"
#include "cosmic-image-capture-source-unstable-v1-client-protocol.h"
#include "cosmic-overlap-notify-unstable-v1-client-protocol.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "ext-image-copy-capture-v1-client-protocol.h"
#include "ext-workspace-v1-client-protocol.h"
// cosmic-toplevel-management's wl_interface table references
// zcosmic_toplevel_handle_v1_interface, which is only defined by
// cosmic-toplevel-info's generated code, so that header must come first.
#include "cosmic-toplevel-info-unstable-v1-client-protocol.h"
#include "cosmic-toplevel-management-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace waybar::modules::cosmic {

class WorkspaceThumbnails;

enum class DisplayMode { Live, Standard };
enum class LabelPosition { Before, After };
enum class CropMode { None, OwnBar, AllBars };
enum class IconsOrientation { Horizontal, Vertical };
enum class IconsPosition { Embedded, BeforeThumbnail, AfterThumbnail, BeforeLabel, AfterLabel };
enum class IconsEdge { Top, Bottom, Left, Right };
enum class IconsAlign { Start, Center, End };

// Shared, module-wide rendering options, parsed once from config.
struct ThumbnailOptions {
  DisplayMode display = DisplayMode::Standard;
  CropMode crop_dead_zones = CropMode::OwnBar;
  bool highlight_show = true;
  int highlight_line_size = 4;
  bool mouse_scroll_enabled = true;
  bool thumbnail_label_show = true;
  std::string thumbnail_label = "{number}";
  Gtk::Align thumbnail_label_halign = Gtk::ALIGN_CENTER;
  Gtk::Align thumbnail_label_valign = Gtk::ALIGN_CENTER;
  bool label_show = true;
  LabelPosition label_position = LabelPosition::Before;
  int spacing = 4;
  bool icons_show = false;
  IconsOrientation icons_orientation = IconsOrientation::Horizontal;
  IconsPosition icons_position = IconsPosition::Embedded;
  // Only used when icons_position is Embedded; icons_orientation (and thus
  // row-vs-column flow) is derived from icons_edge in that case.
  IconsEdge icons_edge = IconsEdge::Bottom;
  IconsAlign icons_align = IconsAlign::Start;
  int icons_size = 20;
  int icons_spacing = 4;
};

// Rectangle (widget-local pixel coordinates) that should remain visible
// after cropping out this bar's own reserved strip.
struct CropRect {
  double x, y, width, height;
};

// A single, continuously-refreshed screencopy of one workspace's contents,
// or a plain CSS-styled placeholder in "standard" display mode.
class Thumbnail {
 public:
  Thumbnail(WorkspaceThumbnails& owner, ext_workspace_handle_v1* workspace);
  ~Thumbnail();

  Gtk::Widget& widget() { return item_box_; }
  ext_workspace_handle_v1* handle() const { return workspace_; }

  // Re-reads name/active state from the owner and redraws.
  void refresh_style();

  // Rebuilds the app-icon row/column from the owner's current toplevel data.
  void refresh_icons();

  // Applies a new (possibly wrap-shrunk) thumbnail size; the draw callback
  // already scales painted content to the widget's actual allocation, so no
  // capture/buffer renegotiation is needed.
  void resize(int width, int height);

  // ext_image_copy_capture_session_v1 events
  void handle_buffer_size(uint32_t width, uint32_t height);
  void handle_shm_format(uint32_t format);
  void handle_constraints_done();
  void handle_session_stopped();

  // ext_image_copy_capture_frame_v1 events
  void handle_frame_ready();
  void handle_frame_failed();

 private:
  void start_session();
  void request_capture();
  bool ensure_buffer();
  void release_buffer();
  void release_session();
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
  bool on_button_press(GdkEventButton* event);
  bool on_capture_timeout();
  void start_worker();
  void stop_worker();
  // Runs for the Thumbnail's whole lifetime on worker_thread_: waits for a job, converts it
  // (reading buffer_data_, guarded by buffer_busy_) and hands the surface back via
  // pending_surface_ + convert_dispatcher_.
  void worker_main();
  // Dispatched onto the main thread once worker_main() finishes a job.
  void on_convert_done();
  void schedule_next_capture();

  WorkspaceThumbnails& owner_;
  ext_workspace_handle_v1* workspace_;

  Gtk::Box item_box_;
  Gtk::Overlay overlay_;
  Gtk::DrawingArea area_;
  Gtk::Label thumbnail_label_;
  Gtk::EventBox thumbnail_label_box_;
  Gtk::Label adjacent_label_;
  Gtk::EventBox adjacent_label_box_;
  Gtk::Box icons_box_;

  ext_image_capture_source_v1* source_ = nullptr;
  ext_image_copy_capture_session_v1* session_ = nullptr;
  ext_image_copy_capture_frame_v1* frame_ = nullptr;

  bool have_shm_format_ = false;
  uint32_t shm_format_ = 0;
  uint32_t buf_width_ = 0;
  uint32_t buf_height_ = 0;

  wl_buffer* buffer_ = nullptr;
  uint8_t* buffer_data_ = nullptr;
  size_t buffer_stride_ = 0;
  size_t buffer_len_ = 0;
  uint32_t alloc_width_ = 0;
  uint32_t alloc_height_ = 0;

  Cairo::RefPtr<Cairo::ImageSurface> surface_;

  bool capture_in_flight_ = false;
  // Delays only the very first capture (offset per thumbnail) so that, with many
  // workspaces on the same refresh interval, their frame-ready callbacks don't all
  // land in the same main-loop iteration and stall the bar.
  bool staggered_ = false;
  sigc::connection timer_conn_;

  // The pixel copy/format-swap runs on a persistent worker_thread_ (started/stopped with the
  // capture session) instead of the GTK main thread. buffer_busy_ (job_mutex_/buffer_free_cv_)
  // guards raw memory safety: release_buffer() waits for it to clear before unmapping
  // buffer_data_. convert_in_flight_ additionally blocks a new capture from being requested
  // until on_convert_done() has consumed the previous job's result on the main thread.
  struct ConvertJob {
    const uint8_t* data = nullptr;
    size_t stride = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
  };
  std::thread worker_thread_;
  std::mutex job_mutex_;
  std::condition_variable job_cv_;
  std::condition_variable buffer_free_cv_;
  ConvertJob pending_job_;
  bool has_job_ = false;
  bool buffer_busy_ = false;
  bool stop_worker_ = false;

  bool convert_in_flight_ = false;
  std::mutex convert_mutex_;
  Cairo::RefPtr<Cairo::ImageSurface> pending_surface_;
  Glib::Dispatcher convert_dispatcher_;
};

class WorkspaceThumbnails final : public AModule {
 public:
  WorkspaceThumbnails(const std::string& id, const waybar::Bar& bar, const Json::Value& config);
  ~WorkspaceThumbnails() override;

  void register_manager(wl_registry* registry, uint32_t name, const char* interface,
                        uint32_t version);

  // Accessors used by Thumbnail
  ext_image_copy_capture_manager_v1* capture_manager() const { return capture_manager_; }
  zcosmic_workspace_image_capture_source_manager_v1* source_manager() const {
    return source_manager_;
  }
  wl_shm* shm() const { return shm_; }
  std::chrono::milliseconds interval() const { return interval_; }
  size_t thumbnail_count() const { return thumbnails_.size(); }
  // Caps how many thumbnails may have a compositor capture + background conversion in
  // flight at once, regardless of refresh interval or workspace count, so a fast interval
  // with many workspaces can't flood the compositor with simultaneous GPU readbacks or
  // saturate CPU/memory bandwidth with concurrent conversions. Non-blocking: callers on the
  // main thread must not wait on a slot, just retry shortly when none is free.
  bool try_acquire_capture_slot() { return capture_slots_.try_acquire(); }
  void release_capture_slot() { capture_slots_.release(); }
  int thumbnail_width() const { return thumb_width_; }
  int thumbnail_height() const { return thumb_height_; }
  const ThumbnailOptions& options() const { return options_; }
  int workspace_wrap_size() const { return workspace_wrap_size_; }
  int thumbnail_min_width() const { return thumb_min_width_; }
  int thumbnail_min_height() const { return thumb_min_height_; }
  Gtk::Orientation bar_orientation() const { return bar_.orientation; }
  // Only the bar's own output has known reserved-space geometry; `handle` identifies
  // which workspace/output this crop is for, so foreign-output thumbnails aren't cropped
  // using this bar's own dead zones.
  CropRect dead_zone_crop(ext_workspace_handle_v1* handle, double width, double height) const;
  void commit() const;

  struct WorkspaceMeta {
    std::string name;
    bool active = false;
    // Grid position within its workspace group, per the compositor's `coordinates`
    // event; empty if the compositor never sent one for this workspace.
    std::vector<uint32_t> coordinates;
  };
  const WorkspaceMeta* meta_for(ext_workspace_handle_v1* handle) const;
  int number_for(ext_workspace_handle_v1* handle) const;

  // One running app/toplevel currently assigned to a workspace.
  struct Toplevel {
    ext_foreign_toplevel_handle_v1* foreign = nullptr;
    zcosmic_toplevel_handle_v1* cosmic = nullptr;
    std::string app_id;
    std::vector<ext_workspace_handle_v1*> workspaces;
  };
  // Toplevels currently assigned to `handle`, in creation order.
  std::vector<const Toplevel*> icons_for(ext_workspace_handle_v1* handle) const;
  const IconLoader& icon_loader() const { return icon_loader_; }
  // Requests the compositor activate (focus) this toplevel.
  void activate_toplevel(zcosmic_toplevel_handle_v1* handle) const;

  // True if `handle` should be highlighted as active right now: either it's
  // the not-yet-confirmed workspace a click/scroll just requested, or (absent
  // a pending request) it's the compositor-confirmed active workspace.
  bool is_effectively_active(ext_workspace_handle_v1* handle) const;
  // Requests a workspace switch and immediately highlights it, ahead of the
  // compositor's confirmation, so the highlight doesn't lag behind the click.
  void request_activate(ext_workspace_handle_v1* handle);

  // ext_workspace_manager_v1 events
  void handle_workspace_group_create(ext_workspace_group_handle_v1* handle);
  void handle_workspace_create(ext_workspace_handle_v1* handle);
  void handle_manager_done();
  void handle_manager_finished();

  // ext_workspace_group_handle_v1 events
  void handle_group_output_enter(ext_workspace_group_handle_v1* group, wl_output* output);
  void handle_group_output_leave(ext_workspace_group_handle_v1* group, wl_output* output);
  void handle_group_workspace_enter(ext_workspace_group_handle_v1* group,
                                    ext_workspace_handle_v1* ws);
  void handle_group_workspace_leave(ext_workspace_group_handle_v1* group,
                                    ext_workspace_handle_v1* ws);
  void handle_group_removed(ext_workspace_group_handle_v1* group);

  // ext_workspace_handle_v1 events
  void handle_workspace_removed(ext_workspace_handle_v1* handle);
  void handle_workspace_name(ext_workspace_handle_v1* handle, const std::string& name);
  void handle_workspace_state(ext_workspace_handle_v1* handle, uint32_t state);
  void handle_workspace_coordinates(ext_workspace_handle_v1* handle,
                                    std::vector<uint32_t> coordinates);

  // Overlap-probe events (zwlr_layer_surface_v1 / zcosmic_overlap_notification_v1)
  void handle_probe_configure(uint32_t serial, uint32_t width, uint32_t height);
  void handle_probe_closed();
  void handle_layer_enter(const std::string& identifier, const std::string& ns, uint32_t exclusive,
                          int32_t x, int32_t y, int32_t width, int32_t height);
  void handle_layer_leave(const std::string& identifier);

  // ext_foreign_toplevel_list_v1 events
  void handle_toplevel_list_toplevel(ext_foreign_toplevel_handle_v1* handle);
  void handle_toplevel_list_finished();

  // ext_foreign_toplevel_handle_v1 events
  void handle_toplevel_closed(ext_foreign_toplevel_handle_v1* handle);
  void handle_toplevel_app_id(ext_foreign_toplevel_handle_v1* handle, const std::string& app_id);
  void handle_toplevel_done(ext_foreign_toplevel_handle_v1* handle);

  // zcosmic_toplevel_handle_v1 events (workspace membership only; everything
  // else about a toplevel is read from its ext_foreign_toplevel_handle_v1)
  void handle_cosmic_workspace_enter(zcosmic_toplevel_handle_v1* handle,
                                     ext_workspace_handle_v1* ws);
  void handle_cosmic_workspace_leave(zcosmic_toplevel_handle_v1* handle,
                                     ext_workspace_handle_v1* ws);

 private:
  void update() override;
  void sync_thumbnails();
  std::vector<ext_workspace_handle_v1*> visible_workspaces() const;
  void refresh_thumbnail(ext_workspace_handle_v1* handle);
  void refresh_icons(ext_workspace_handle_v1* handle);
  bool on_scroll(GdkEventScroll* event);

  // Result of applying workspace-wrap-size to a given visible-workspace count.
  struct WrapLayout {
    int lines = 1;
    int items_per_line = 0;
    int thumb_width = 0;
    int thumb_height = 0;
  };
  WrapLayout compute_wrap_layout(size_t count) const;

  // Overlap-probe: an invisible, full-output layer-surface used solely to ask
  // the compositor (via cosmic-overlap-notify) which other layer-surfaces
  // reserve screen space, so crop-dead-zones can crop them out of previews.
  void start_overlap_probe();
  void stop_overlap_probe();

  struct EdgeClips {
    uint32_t top = 0, bottom = 0, left = 0, right = 0;
    bool operator==(const EdgeClips&) const = default;
  };
  EdgeClips compute_edge_clips() const;
  mutable EdgeClips last_logged_clips_{};
  mutable bool logged_clips_once_ = false;

  struct GroupInfo {
    ext_workspace_group_handle_v1* handle;
    std::vector<wl_output*> outputs;
    std::vector<ext_workspace_handle_v1*> workspaces;
  };

  struct ReservedRect {
    std::string ns;
    uint32_t exclusive;
    int32_t x, y, width, height;
  };

  const waybar::Bar& bar_;
  Gtk::Grid box_;

  wl_shm* shm_ = nullptr;
  ext_workspace_manager_v1* workspace_manager_ = nullptr;
  ext_image_copy_capture_manager_v1* capture_manager_ = nullptr;
  zcosmic_workspace_image_capture_source_manager_v1* source_manager_ = nullptr;
  wl_compositor* compositor_ = nullptr;
  zwlr_layer_shell_v1* layer_shell_ = nullptr;
  zcosmic_overlap_notify_v1* overlap_notify_ = nullptr;
  ext_foreign_toplevel_list_v1* foreign_toplevel_list_ = nullptr;
  zcosmic_toplevel_info_v1* toplevel_info_manager_ = nullptr;
  zcosmic_toplevel_manager_v1* toplevel_manager_ = nullptr;
  wl_seat* seat_ = nullptr;

  wl_surface* probe_surface_ = nullptr;
  zwlr_layer_surface_v1* probe_layer_surface_ = nullptr;
  zcosmic_overlap_notification_v1* probe_notification_ = nullptr;
  wl_buffer* probe_buffer_ = nullptr;
  uint32_t probe_buffer_width_ = 0;
  uint32_t probe_buffer_height_ = 0;
  std::unordered_map<std::string, ReservedRect> reserved_;

  std::vector<GroupInfo> groups_;
  std::vector<ext_workspace_handle_v1*> workspaces_;
  std::unordered_map<ext_workspace_handle_v1*, WorkspaceMeta> meta_;
  std::unordered_map<ext_workspace_handle_v1*, std::unique_ptr<Thumbnail>> thumbnails_;

  // Keyed by the ext_foreign_toplevel_handle_v1 (created first); cosmic_to_foreign_
  // lets the zcosmic_toplevel_handle_v1-side listeners (workspace membership) find
  // their way back to the same entry.
  std::unordered_map<ext_foreign_toplevel_handle_v1*, Toplevel> toplevels_;
  std::unordered_map<zcosmic_toplevel_handle_v1*, ext_foreign_toplevel_handle_v1*>
      cosmic_to_foreign_;
  IconLoader icon_loader_;

  ext_workspace_handle_v1* pending_active_ = nullptr;
  sigc::connection pending_active_timeout_;

  ThumbnailOptions options_;
  bool all_outputs_ = false;
  std::chrono::milliseconds interval_{1000};
  int thumb_width_ = 160;
  int thumb_height_ = 90;
  int thumb_min_width_ = 0;
  int thumb_min_height_ = 0;
  int workspace_wrap_size_ = 0;
  bool needs_sync_ = false;
  std::counting_semaphore<> capture_slots_;
};

}  // namespace waybar::modules::cosmic
