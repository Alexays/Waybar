test_inc = include_directories('../include')
test_dep = [
    catch2,
    fmt,
    jsoncpp,
    spdlog,
]

config_test = executable(
    'config_test',
    'config.cpp',
    '../src/config.cpp',
    dependencies: test_dep,
    include_directories: test_inc,
)

test(
    'Configuration test',
    config_test,
    workdir: meson.source_root(),
)
