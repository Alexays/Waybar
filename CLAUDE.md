# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System & Commands

Waybar uses Meson as its build system with C++20 standard.

### Initial Setup
```bash
meson setup build
```

### Building
```bash
ninja -C build
```

### Running (from build directory)
```bash
./build/waybar
```

### Installing
```bash
ninja -C build install
```

### Linting
```bash
ninja -C build tidy
```

### Testing
```bash
# Run all tests
meson test -C build

# Run tests from the build directory
./build/waybar_test
```

### Running Single Tests
The project uses Catch2 for testing. To run specific tests:
```bash
# Run tests matching a pattern
./build/waybar_test "test name pattern"

# Run tests from a specific file/suite
./build/waybar_test "[tag]"
```

Test files are located in `test/` directory with subdirectories for specific modules (e.g., `test/utils/`, `test/hyprland/`, `test/config/`).

## Code Architecture

### High-Level Overview

Waybar follows a modular architecture with a clear separation between the bar container, client management, and individual feature modules:

1. **Client** (`src/client.cpp`, `include/client.hpp`) - Singleton that manages the entire application lifecycle, Wayland display connection, CSS styling, and bar instances
2. **Bar** (`src/bar.cpp`, `include/bar.hpp`) - Represents a single bar instance on an output/monitor. Contains three horizontal boxes (left, center, right) that hold modules
3. **Factory** (`src/factory.cpp`, `include/factory.hpp`) - Creates module instances based on configuration using a factory pattern
4. **Modules** - Individual widgets/features that extend base classes

### Module Hierarchy

All modules inherit from base classes that provide common functionality:

- **IModule** - Pure interface defining the module contract
- **AModule** (`include/AModule.hpp`) - Abstract base providing event handling (click, scroll, hover), GTK event box, and update dispatcher
- **ALabel** - Extends AModule for text-based modules using Gtk::Label
- **AIconLabel** - Extends ALabel adding icon support
- **AAppIconLabel** - Extends AIconLabel for application icons
- **ASlider** - Extends AModule for slider-based modules (e.g., volume, brightness)

When creating a new module:
1. Inherit from the appropriate base class (typically ALabel or AIconLabel)
2. Implement the `update()` method to refresh the display
3. Register module creation in `src/factory.cpp`
4. Add module configuration to `meson.build`

### Compositor-Specific Backends

Several window managers have dedicated backend implementations that handle IPC/socket communication:

- **Hyprland** (`src/modules/hyprland/backend.cpp`) - Uses singleton IPC class for socket communication with event registration system
- **Sway** (`src/modules/sway/ipc/client.cpp`) - IPC client for Sway-specific features
- **River** - River compositor support
- **Niri** (`src/modules/niri/backend.cpp`) - Niri compositor backend
- **Wayfire** (`src/modules/wayfire/backend.cpp`) - Wayfire compositor backend
- **DWL** - DWL IPC support

These backends typically implement singleton patterns and provide event-based callbacks for workspace/window changes.

### Wayland Protocols

Protocol definitions in `protocol/` directory are compiled into C code using `wayland-scanner`:
- XDG Shell, XDG Output protocols (from wayland-protocols package)
- Custom protocols: wlr-foreign-toplevel, river-status/control, dwl-ipc
- DBus protocols for system tray (StatusNotifierItem/Watcher)
- ext-workspace protocol (requires wayland-protocols >= 1.39)

The `protocol/meson.build` generates both source files and headers which are compiled into a static library (`lib_client_protos`).

### Configuration System

- JSON-based configuration (`resources/config.jsonc`)
- CSS styling (`resources/style.css`)
- Configuration parsing in `src/config.cpp` using jsoncpp
- Per-output configuration support
- Module-specific configuration passed via `Json::Value` objects

### Threading Model

- Main GTK thread for UI updates
- Background threads for module data collection
- `Glib::Dispatcher` (`dp` member in AModule) for thread-safe UI updates from worker threads
- Each compositor backend typically runs a separate thread for socket listening

## Code Style

The project follows Google's C++ Style Guide with these settings:
- Clang-format configuration in `.clang-format`
- Based on Google style
- 100 character column limit
- Snake_case for variables, PascalCase for classes
- Use `clang-tidy` for static analysis

## Build Options

Configure builds with meson options (see `meson_options.txt`):

```bash
# Example: build without pulseaudio support
meson setup build -Dpulseaudio=disabled

# Example: enable experimental features
meson setup build -Dexperimental=true

# Example: enable niri compositor support
meson setup build -Dniri=true
```

Key options: `libnl`, `libudev`, `libevdev`, `pulseaudio`, `mpd`, `systemd`, `dbusmenu-gtk`, `jack`, `wireplumber`, `cava`, `niri`, `tests`, `man-pages`

## Module Development

When adding or modifying modules:

1. **Platform-specific code**: Check for OS macros (`HAVE_CPU_LINUX`, `HAVE_CPU_BSD`, `HAVE_MEMORY_LINUX`, etc.)
2. **Conditional compilation**: Feature flags like `HAVE_LIBNL`, `HAVE_LIBPULSE`, `HAVE_HYPRLAND` control module availability
3. **Update locations**:
   - Add source files to `src_files` in `meson.build`
   - Add man pages to `man_files` in `meson.build`
   - Add necessary dependencies
   - Register in Factory pattern (`src/factory.cpp`)

## Utilities

Common utilities in `src/util/` and `include/util/`:
- `SafeSignal.hpp` - Thread-safe signal handling
- `command.hpp` - Execute external commands
- `icon_loader.hpp` - Load icons from themes
- `css_reload_helper.hpp` - Hot reload CSS
- `portal.hpp` - Desktop portal integration
- `json.hpp` - JSON parsing utilities
- `regex_collection.hpp` - Regex pattern collections
- `backend_common.hpp` - Shared backend functionality

## Dependencies

Core dependencies (see `meson.build` for versions):
- gtkmm-3.0 (>= 3.22.0)
- gtk-layer-shell (>= 0.9.0) - For Wayland layer-shell
- jsoncpp (>= 1.9.2)
- fmt (>= 8.1.1)
- spdlog (>= 1.15.2)
- wayland-client, wayland-protocols
- sigc++-2.0

Optional dependencies enable specific modules (network, audio, battery monitoring, etc.)
