test_inc = include_directories('../include')
test_dep = [
    catch2,
    fmt,
    gtkmm,
    jsoncpp,
    spdlog,
]

waybar_test = executable(
    'waybar_test',
    'main.cpp',
    'SafeSignal.cpp',
    'config.cpp',
    '../src/config.cpp',
    dependencies: test_dep,
    include_directories: test_inc,
)

test(
    'waybar',
    waybar_test,
    workdir: meson.source_root(),
)
