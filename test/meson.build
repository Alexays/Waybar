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
    'sleeper_thread.cpp',
)

if tz_dep.found()
  test_dep += tz_dep
  test_src += files('date.cpp')
endif

waybar_test = executable(
    'waybar_test',
    test_src,
    dependencies: [
        waybar_lib
    ] + test_dep + third_party_dependencies,
    include_directories: test_inc,
)

test(
    'waybar',
    waybar_test,
    workdir: meson.source_root(),
)
