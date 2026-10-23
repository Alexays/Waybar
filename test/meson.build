test_inc = include_directories('../include')

test_dep = [
    catch2,
    fmt,
    gtkmm,
    jsoncpp,
    spdlog,
]

test_src = files(
    'main.cpp',
    'config.cpp',
    '../src/config.cpp',
)

waybar_test = executable(
    'waybar_test',
    test_src,
    dependencies: test_dep,
    include_directories: test_inc,
)

test(
    'waybar',
    waybar_test,
    workdir: meson.project_source_root(),
)

subdir('utils')
subdir('hyprland')
