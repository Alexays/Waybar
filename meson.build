project(
    'waybar', 'cpp', 'c',
    version: '0.0.3',
    license: 'MIT',
    default_options : ['cpp_std=c++17'],
)

cpp_args = []
cpp_link_args = []

if false # libc++
    cpp_args += ['-stdlib=libc++']
    cpp_link_args += ['-stdlib=libc++', '-lc++abi']

    cpp_link_args += ['-lc++fs']
else
    # TODO: For std::filesystem in libstdc++. Still unstable? Or why is it not in libstdc++ proper yet?
    cpp_link_args += ['-lstdc++fs']
endif

add_global_arguments(cpp_args, language : 'cpp')
add_global_link_arguments(cpp_link_args, language : 'cpp')

thread_dep = dependency('threads')
libinput = dependency('libinput')
fmt = dependency('fmt', fallback: ['fmtlib', 'fmt_dep'])
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols')
wlroots = dependency('wlroots', fallback: ['wlroots', 'wlroots'])
gtkmm = dependency('gtkmm-3.0')
jsoncpp = dependency('jsoncpp')
sigcpp = dependency('sigc++-2.0')
libnl = dependency('libnl-3.0')
libnlgen = dependency('libnl-genl-3.0')
libpulse = dependency('libpulse')

subdir('protocol')

executable(
    'waybar',
    run_command('find', './src', '-name', '*.cpp').stdout().strip().split('\n'),
    dependencies: [
        thread_dep,
        wlroots,
        client_protos,
        wayland_client,
        fmt,
        sigcpp,
        jsoncpp,
        libinput,
        wayland_cursor,
        gtkmm,
        libnl,
        libnlgen,
        libpulse,
    ],
    include_directories: [include_directories('include')],
    install: true,
)

install_data(
    './resources/config',
    './resources/style.css',
    install_dir: '/etc/xdg/waybar',
)
