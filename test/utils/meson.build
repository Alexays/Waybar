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
    'format.cpp',
    'sleeper_thread.cpp',
    'command.cpp',
    'command_line_stream.cpp',
    'css_reload_helper.cpp',
    '../../src/util/css_reload_helper.cpp',
    '../../src/util/command_line_stream.cpp',
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

if is_linux and libudev.found()
  umockdev = dependency('umockdev-1.0', required: false)
  umockdev_wrapper = find_program('umockdev-wrapper', required: false)
  if umockdev.found() and umockdev_wrapper.found()
    backlight_test = executable(
      'backlight_test',
      '../main.cpp',
      'backlight_backend.cpp',
      '../../src/util/backlight_backend.cpp',
      dependencies: test_dep + [libudev, umockdev],
      include_directories: test_inc,
      cpp_args: ['-UHAVE_LOGIN_PROXY'],
      link_args: [
        '-Wl,--wrap=udev_device_get_sysattr_value',
        '-Wl,--wrap=udev_monitor_receive_device',
      ],
    )
    test('backlight', umockdev_wrapper, args: [backlight_test])
  endif
endif
