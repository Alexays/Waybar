# Contributing to Waybar

Thanks for helping improve Waybar! This guide covers the essentials.

## Building for development

```bash
meson setup build
ninja -C build
./build/waybar          # run your build directly
```

Enable all optional modules while developing:

```bash
meson setup build -Dexperimental=true
```

## Code style

Waybar follows [Google's C++ style guide](https://google.github.io/styleguide/cppguide.html).
Format your changes before committing:

```bash
clang-format -i <files>
```

CI runs `clang-format` and a full build on Linux and FreeBSD — please make sure
both pass.

## Documentation

Module documentation lives in [`man/`](man) as scdoc man pages, **not** in the
wiki. Editing a man page and merging to `master` regenerates the matching wiki
page automatically (see [`.github/wiki`](.github/wiki)). When you add a module,
add its man page and a line in [`.github/wiki/mapping.json`](.github/wiki/mapping.json).

## Pull requests

- Branch from `master` and keep each PR focused on one change.
- Describe what changed and why; link any related issues.
- Add or update the man page for every user-facing option you introduce.
- Build and test against the module(s) you touched.

Have fun :)

---

# Coding Conventions

## 1. Language & Build
- **Standard**: C++20.
- **Build system**: Meson (`meson.build`). Project version is defined there.
- **Compiler flags**: Added via `add_project_arguments()` in Meson. Feature flags use `HAVE_*` / `WANT_*` prefixes (e.g. `-DHAVE_NIRI`, `-DHAVE_HYPRLAND`, `-DHAVE_LIBUDEV`).

## 2. Formatting
- **Tool**: `.clang-format` is checked in. **Never bypass it.**
- **Style**: Google base style.
- **Indent**: 2 spaces. No tabs.
- **Column limit**: 100.
- **Braces**: K&R (opening brace on the same line).
- **Declaration alignment**: Disabled (`AlignConsecutiveDeclarations: false`).
- **Pointer/reference alignment**: Left (`const Json::Value& config`, `int* ptr`, not `int *ptr`).

## 3. Naming

### Files
- Match the primary exported class exactly: `AAppIconLabel.hpp`, `workspaces.cpp`, `backlight_backend.hpp`.
- Corresponding header and source should live in predictable paths:
  - `include/<module/path>.hpp`
  - `src/<module/path>.cpp`

### Types
- **Classes / Structs**: `PascalCase`.
  - Abstract base classes are prefixed with `A` (e.g., `AModule`, `ALabel`, `AIconLabel`, `AAppIconLabel`).
- **Enums / Enum classes**: `PascalCase` name.
  - Enumerators: `UPPER_SNAKE_CASE` (e.g., `SCROLL_DIR::NONE`, `KillSignalAction::RELOAD`, `ChangeType::Increase`).
- **Concepts / Type aliases**: `PascalCase`.

### Variables
- **Member variables**: `snake_case_` with a **trailing underscore**.
  - Examples: `config_`, `bar_`, `label_`, `app_icon_size_`, `distance_scrolled_y_`, `on_updated_cb_`.
- **Function parameters & locals**: `snake_case` (no trailing underscore).
  - Examples: `workspace_data`, `should_refresh`, `app_identifier`, `preferred_device`.
- **Static / constexpr constants**: `UPPER_SNAKE_CASE` or descriptive `kPascalCase`.
  - Examples: `MODULE_CLASS`, `EPOLL_MAX_EVENTS`, `kExecFailureExitCode`.

### Functions & Methods
- **Free functions**: `snake_case`.
  - Examples: `sanitize_string()`, `rewrite_string()`, `get_total_memory()`, `best_device()`.
- **Class methods**: `lowerCamelCase`.
  - Examples: `update()`, `tooltipEnabled()`, `handleScroll()`, `getScrollDir()`, `resolveFormat()`, `setBrightness()`.
- **Virtual overrides**: Mark with `override` (and `final` where applicable). Header signatures often use a trailing return type:
  ```cpp
  auto update() -> void override;
  auto refresh(int should_refresh) -> void;
  ```

### Namespaces
- All lowercase, nested by module path:
  ```cpp
  namespace waybar { }
  namespace waybar::modules::niri { }
  namespace waybar::util { }
  ```
- Close every namespace with a comment:
  ```cpp
  }  // namespace waybar::modules::niri
  ```

## 4. Includes & Headers
- Use `#pragma once` in all project headers.
- Include order in `.cpp` files:
  1. Corresponding header first.
  2. Blank line.
  3. External library headers (`<fmt/...>`, `<spdlog/...>`, `<gtkmm/...>`, `<json/json.h>`).
  4. Standard library headers (`<algorithm>`, `<vector>`, `<memory>`).
  5. Blank line.
  6. Other project headers (`"util/..."`, `"modules/..."`).
- Do not use `using namespace` in headers. In `.cpp` files it is acceptable for narrow scopes (e.g., `using namespace std::literals::chrono_literals;`).
- Headers that expose standard-library types in their public interface (e.g. `std::chrono::milliseconds` as a return type or `std::vector<T>` as a member) must `#include` the corresponding standard header directly. Do not rely on transitive includes from other headers.

## 5. Class & Module Design

### Base Class Patterns
- All UI modules ultimately derive from `AModule` (and often `ALabel` or `AIconLabel`).
- Accept configuration in constructors:
  ```cpp
  MyModule(const Json::Value& config, const std::string& name, const std::string& id, ...);
  ```

### Signals & Threading
- Use `Glib::Dispatcher` (via `waybar::SafeSignal`) to marshal work to the GTK main thread.
- Use `sigc::signal` for normal GTK++ signals.
- If a scope must not be interrupted by `pthread_cancel`, guard it with `waybar::util::CancellationGuard`.

### State / IPC
- Modules that talk to a compositor often implement a small `EventHandler` interface (`onEvent(...)`) and delegate to a singleton backend (e.g., `gIPC`).

### RAII
- Prefer `std::unique_ptr` with custom deleters over raw `new/delete` for C-API resources (see `ScopedFd`, `UdevDeleter`, `UdevDeviceDeleter`, `ScopeGuard`).

## 6. JSON Configuration
- Every module receives `const Json::Value& config` (usually as the first constructor argument).
- Always validate node type before reading:
  ```cpp
  if (config_["sort-by-id"].isBool()) { ... }
  if (config.isMember("window-rewrite-default") && config["window-rewrite-default"].isString()) { ... }
  ```
- Use `waybar::util::JsonParser` if you need to pre-process JSON with non-standard escape sequences.

## 7. String & UI Formatting
- Use `fmt::format` / `fmt::join` for all string composition.
- Use `fmt::dynamic_format_arg_store<fmt::format_context>` when building arguments dynamically.
- Custom `fmt::formatter` specializations are allowed for domain types (e.g., `Glib::ustring`, project enums).
- Sanitize arbitrary text before inserting into Pango markup with `waybar::util::sanitize_string`.
- Use `waybar::util::rewriteString` for user-configurable regex rewrites.
- Truncate UTF-8 safely with `waybar::util::utf8_truncate` / `utf8_width`.

## 8. Error Handling & Logging
- Use `spdlog` for all logging:
  - `spdlog::error("Context: {}", e.what());`
  - `spdlog::warn("Deprecated key '{}', prefer '{}'", old, replacement);`
  - `spdlog::debug("State changed to {}", value);`
- Throw `std::runtime_error` (or similar) for fatal initialization failures that should bubble up to `main()`.

## 9. GTK / Glib Patterns
- Prefer gtkmm-3.0 types (`Gtk::Button`, `Gtk::Label`, `Gdk::Pixbuf`, `Glib::RefPtr`, `Glib::ustring`) over raw C GTK APIs.
- Access the default icon theme through thread-safe wrappers if off the main thread (`DefaultGtkIconThemeWrapper`).
- Tooltips and labels should respect the module `tooltip` toggle (see `tooltipEnabled()` in `AModule`).

## 10. Platform Portability
- Isolate platform-specific code in dedicated files (e.g., `linux.cpp`, `bsd.cpp`).
- Use preprocessor guards for platform differences (`#if defined(__FreeBSD__)`, `#if defined(HAVE_LIBNL)`).
- Keep the common interface in a shared header or base class.

## 11. Thread Safety & Cross-Thread Communication
- GTK is strictly single-threaded. Never emit raw `sigc::signal` from background threads.
- Use `waybar::SafeSignal<T...>` to marshal events from worker threads to the GTK main loop.
- When a module manages background threads, use `std::mutex`, `std::recursive_mutex`, or atomic variables to protect shared state, and ensure the destructor joins or synchronizes with those threads before destroying resources.

## 12. Unsafe Patterns to Avoid
- Do not use `strcpy`, `strcat`, or `sprintf` into fixed-size buffers (e.g. `char buf[PATH_MAX]`). Prefer `std::string`, `std::vector<char>`, or `std::array` with bounds-safe operations.
- When passing a `std::vector<char>` buffer to a C API that expects a mutable `char*` string, always ensure the buffer is null-terminated and clamp the written length to `size() - 1`. Never use `std::copy` from an unbounded source into a fixed-size buffer.

## 13. Singleton Lifetime
- Singletons or objects with process-wide lifetime must not store references (`&`) or pointers to objects with shorter lifetime (e.g., configuration trees, GTK widgets, or bar instances) unless they are explicitly notified of destruction. Prefer storing configuration by value (`Json::Value`, `std::string`, etc.) if the singleton outlives the config loader.
