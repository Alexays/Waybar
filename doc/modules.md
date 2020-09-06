# Writing Modules

A Waybar module is usually a class derived from the `waybar::ALabel` class,
which is itself derived from `waybar::AModule`. Modules can also be derived
directly from `waybar::AModule`.

Module source files should go in `src/modules/`, while their headers should be
in `include/modules/`. The source file should be added to `src_files` in
`meson.build` according the appropriate feature flag, which should be listed in
`meson_options.txt`. The module instantiation should be added in
`src/factory.cpp`, while the module's header should be included in
`include/factory.hpp`.

A basic module will write text into its `label_` member (inherited from
`ALabel`) using its `set_markup()` method. You will usually want to have some
way of changing the text in said label to respond to external events, which can
be achieved by defining `ALabel`'s virtual `auto update() -> void` method for
your class. This function will be called whenever the `emit()` method for the
`dp` member (inherited from `AModule`) is called. The `update()` method should
also call the `update()` method from its parent class: `ALabel::update();`.

The simplest way of reacting to external events is by creating a polling module.
This can be done by initializing `ALabel` with some value for the `interval`
parameter and adding an `util::Sleeper_Thread` variable to your class, which
requires including the `util/sleeper_thread.hpp` header. The sleeper thread
should then be configured similarly to how it's done in `src/modules/clock.cpp`.
