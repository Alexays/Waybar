project(
    'waybar', 'cpp', 'c',
    version: '0.6.9',
    license: 'MIT',
    default_options : [
        'cpp_std=c++17',
        'buildtype=release',
        'default_library=static'
    ],
)

cpp_args = []
cpp_link_args = []

if get_option('libcxx')
    cpp_args += ['-stdlib=libc++']
    cpp_link_args += ['-stdlib=libc++', '-lc++abi']

    cpp_link_args += ['-lc++fs']
else
    cpp_link_args += ['-lstdc++fs']
endif

compiler = meson.get_compiler('cpp')
git = find_program('git', required: false)

if not git.found()
    add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'cpp')
else
    git_commit_hash = run_command([git.path(), 'describe', '--always', '--tags']).stdout().strip()
    git_branch = run_command([git.path(), 'rev-parse', '--abbrev-ref', 'HEAD']).stdout().strip()
    version = '"@0@ (" __DATE__ ", branch \'@1@\')"'.format(git_commit_hash, git_branch)
    add_project_arguments('-DVERSION=@0@'.format(version), language: 'cpp')
endif

if not compiler.has_header('filesystem')
    if compiler.has_header('experimental/filesystem')
        add_project_arguments('-DFILESYSTEM_EXPERIMENTAL', language: 'cpp')
    else
        add_project_arguments('-DNO_FILESYSTEM', language: 'cpp')
        warning('No filesystem header found, some modules may not work')
    endif
endif

add_global_arguments(cpp_args, language : 'cpp')
add_global_link_arguments(cpp_link_args, language : 'cpp')

thread_dep = dependency('threads')
libinput = dependency('libinput')
fmt = dependency('fmt', version : ['>=5.3.0'], fallback : ['fmt', 'fmt_dep'])
spdlog = dependency('spdlog', version : ['>=1.3.1'], fallback : ['spdlog', 'spdlog_dep'])
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols')
gtkmm = dependency('gtkmm-3.0')
dbusmenu_gtk = dependency('dbusmenu-gtk3-0.4', required: get_option('dbusmenu-gtk'))
giounix = dependency('gio-unix-2.0', required: get_option('dbusmenu-gtk'))
jsoncpp = dependency('jsoncpp')
sigcpp = dependency('sigc++-2.0')
libnl = dependency('libnl-3.0', required: get_option('libnl'))
libnlgen = dependency('libnl-genl-3.0', required: get_option('libnl'))
libpulse = dependency('libpulse', required: get_option('pulseaudio'))
libudev = dependency('libudev', required: get_option('libudev'))
libmpdclient = dependency('libmpdclient', required: get_option('mpd'))

src_files = files(
    'src/factory.cpp',
    'src/ALabel.cpp',
    'src/modules/memory.cpp',
    'src/modules/battery.cpp',
    'src/modules/clock.cpp',
    'src/modules/custom.cpp',
    'src/modules/cpu.cpp',
	'src/modules/idle_inhibitor.cpp',
    'src/modules/temperature.cpp',
    'src/main.cpp',
    'src/bar.cpp',
    'src/client.cpp'
)

if true # find_program('sway', required : false).found()
    add_project_arguments('-DHAVE_SWAY', language: 'cpp')
    src_files += [
        'src/modules/sway/ipc/client.cpp',
        'src/modules/sway/mode.cpp',
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
        'src/modules/sni/watcher.cpp',
        'src/modules/sni/host.cpp',
        'src/modules/sni/item.cpp'
    )
endif

if libudev.found()
    add_project_arguments('-DHAVE_LIBUDEV', language: 'cpp')
    src_files += 'src/modules/backlight.cpp'
endif

if libmpdclient.found()
    add_project_arguments('-DHAVE_LIBMPDCLIENT', language: 'cpp')
    src_files += 'src/modules/mpd.cpp'
endif

subdir('protocol')

executable(
    'waybar',
    src_files,
    dependencies: [
        thread_dep,
        client_protos,
        wayland_client,
        fmt,
        spdlog,
        sigcpp,
        jsoncpp,
        libinput,
        wayland_cursor,
        gtkmm,
        dbusmenu_gtk,
        giounix,
        libnl,
        libnlgen,
        libpulse,
        libudev,
        libmpdclient
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
