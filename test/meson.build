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
    'SafeSignal.cpp',
    'config.cpp',
    '../src/config.cpp',
)

if tz_dep.found()
  test_dep += tz_dep
  test_src += files('waybar_time.cpp')
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
    workdir: meson.source_root(),
)
