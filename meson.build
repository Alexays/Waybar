project(
    'waybar', 'cpp', 'c',
    version: '0.1.2',
    license: 'MIT',
    default_options : [
        'cpp_std=c++17',
        'buildtype=release'
    ],
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
dbusmenu_gtk = dependency('dbusmenu-gtk3-0.4', required: get_option('dbusmenu-gtk'))
giounix = dependency('gio-unix-2.0', required: get_option('dbusmenu-gtk'))
jsoncpp = dependency('jsoncpp')
sigcpp = dependency('sigc++-2.0')
libnl = dependency('libnl-3.0', required: get_option('libnl'))
libnlgen = dependency('libnl-genl-3.0', required: get_option('libnl'))
libpulse = dependency('libpulse', required: get_option('pulseaudio'))

src_files = files(
    'src/factory.cpp',
    'src/ALabel.cpp',
    'src/modules/memory.cpp',
    'src/modules/battery.cpp',
    'src/modules/clock.cpp',
    'src/modules/custom.cpp',
    'src/modules/cpu.cpp',
    'src/main.cpp',
    'src/bar.cpp',
    'src/client.cpp'
)

if find_program('sway', required : false).found()
    add_project_arguments('-DHAVE_SWAY', language: 'cpp')
    src_files += [
        'src/modules/sway/ipc/client.cpp',
        'src/modules/sway/window.cpp',
        'src/modules/sway/workspaces.cpp'
    ]
endif

if libnl.found() and libnlgen.found()
    add_project_arguments('-DHAVE_LIBNL', language: 'cpp')
    src_files += 'src/modules/network.cpp'
endif

if libpulse.found()
    add_project_arguments('-DHAVE_LIBPULSE', language: 'cpp')
    src_files += 'src/modules/pulseaudio.cpp'
endif

if dbusmenu_gtk.found()
    add_project_arguments('-DHAVE_DBUSMENU', language: 'cpp')
    src_files += files(
        'src/modules/sni/tray.cpp',
        'src/modules/sni/snw.cpp',
        'src/modules/sni/snh.cpp',
        'src/modules/sni/sni.cpp'
    )
endif

subdir('protocol')

executable(
    'waybar',
    src_files,
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
        dbusmenu_gtk,
        giounix,
        libnl,
        libnlgen,
        libpulse
    ],
    include_directories: [include_directories('include')],
    install: true,
)

install_data(
    './resources/config',
    './resources/style.css',
    install_dir: join_paths(get_option('out'), 'etc/xdg/waybar')
)

clangtidy = find_program('clang-tidy', required: false)

if clangtidy.found()
    run_target(
        'tidy',
        command: [
            clangtidy,
            '-checks=*,-fuchsia-default-arguments',
            '-p', meson.build_root()
        ] + src_files)
endif
