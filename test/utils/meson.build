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
    '../config.cpp',
    '../../src/config.cpp',
    'JsonParser.cpp',
    'SafeSignal.cpp',
    'css_reload_helper.cpp',
    '../../src/util/css_reload_helper.cpp',
)

if tz_dep.found()
  test_dep += tz_dep
  test_src += files('date.cpp')
endif

utils_test = executable(
    'utils_test',
    test_src,
    dependencies: test_dep,
    include_directories: test_inc,
)

test(
    'utils',
    utils_test,
    workdir: meson.project_source_root(),
)
