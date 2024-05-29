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
    'JsonParser.cpp',
    'SafeSignal.cpp',
    'config.cpp',
    'css_reload_helper.cpp',
    '../src/config.cpp',
    '../src/util/css_reload_helper.cpp',
)

if tz_dep.found()
  test_dep += tz_dep
  test_src += files('date.cpp')
endif

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

subdir('hyprland')
