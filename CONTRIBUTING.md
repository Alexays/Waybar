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
