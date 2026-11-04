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
    'backlight_brightness.cpp',
    'SafeSignal.cpp',
    'format.cpp',
    'string.cpp',
    'tooltip.cpp',
    'sleeper_thread.cpp',
    'command.cpp',
    'command_line_stream.cpp',
    'css_class.cpp',
    'css_reload_helper.cpp',
    '../../src/util/css_reload_helper.cpp',
    '../../src/util/command_line_stream.cpp',
)

test_src += files('regex_collection.cpp', '../../src/util/regex_collection.cpp')
test_src += files('rewrite_string.cpp', '../../src/util/rewrite_string.cpp')

if libnl.found() and libnlgen.found()
  test_dep += [libnl, libnlgen]
  test_src += files('netlink.cpp', '../../src/util/netlink.cpp')
endif

if tz_dep.found()
  test_dep += tz_dep
  test_src += files('date.cpp')
endif

test_dep += giounix
test_src += files(
    'icon_loader.cpp',
    '../../src/util/icon_loader.cpp',
    '../../src/util/gtk_icon.cpp',
)

test_src += files('utf8_string.cpp', '../../src/util/utf8_string.cpp')

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
