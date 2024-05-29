test_inc = include_directories('../../include')

test_dep = [
    catch2,
    fmt,
    gtkmm,
    jsoncpp,
    spdlog,
]

test_src = files(
    '../main.cpp',
    '../JsonParser.cpp',
    '../SafeSignal.cpp',
    '../config.cpp',
    '../css_reload_helper.cpp',
    '../../src/config.cpp',
    '../../src/util/css_reload_helper.cpp',
    'backend.cpp',
    '../../src/modules/hyprland/backend.cpp'
)

hyprland_test = executable(
    'hyprland_test',
    test_src,
    dependencies: test_dep,
    include_directories: test_inc,
)

test(
    'hyprland',
    hyprland_test,
    workdir: meson.project_source_root(),
)
