project(
    'waybar', 'cpp', 'c',
    version: '0.9.16',
    license: 'MIT',
    meson_version: '>= 0.49.0',
    default_options : [
        'cpp_std=c++17',
        'buildtype=release',
        'default_library=static'
    ],
)

compiler = meson.get_compiler('cpp')

cpp_args = []
cpp_link_args = []

if get_option('libcxx')
    cpp_args += ['-stdlib=libc++']
    cpp_link_args += ['-stdlib=libc++', '-lc++abi']
endif

if compiler.has_link_argument('-lc++fs')
    cpp_link_args += ['-lc++fs']
elif compiler.has_link_argument('-lc++experimental')
    cpp_link_args += ['-lc++experimental']
elif compiler.has_link_argument('-lstdc++fs')
    cpp_link_args += ['-lstdc++fs']
endif

git = find_program('git', native: true, required: false)

if not git.found()
    add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'cpp')
else
    git_path = run_command([git.path(), 'rev-parse', '--show-toplevel']).stdout().strip()
    if meson.source_root() == git_path
        git_commit_hash = run_command([git.path(), 'describe', '--always', '--tags']).stdout().strip()
        git_branch = run_command([git.path(), 'rev-parse', '--abbrev-ref', 'HEAD']).stdout().strip()
        version = '"@0@ (branch \'@1@\')"'.format(git_commit_hash, git_branch)
        add_project_arguments('-DVERSION=@0@'.format(version), language: 'cpp')
    else
        add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'cpp')
    endif
endif

if not compiler.has_header('filesystem')
    if compiler.has_header('experimental/filesystem')
        add_project_arguments('-DFILESYSTEM_EXPERIMENTAL', language: 'cpp')
    else
        add_project_arguments('-DNO_FILESYSTEM', language: 'cpp')
        warning('No filesystem header found, some modules may not work')
    endif
endif

code = '''
#include <langinfo.h>
#include <locale.h>
int main(int argc, char** argv) {
    locale_t locale = newlocale(LC_ALL, "en_US.UTF-8", nullptr);
    char* str;
    str = nl_langinfo_l(_NL_TIME_WEEK_1STDAY, locale);
    str = nl_langinfo_l(_NL_TIME_FIRST_WEEKDAY, locale);
    freelocale(locale);
    return 0;
}
'''
if compiler.links(code, name : 'nl_langinfo with _NL_TIME_WEEK_1STDAY, _NL_TIME_FIRST_WEEKDAY')
    add_project_arguments('-DHAVE_LANGINFO_1STDAY', language: 'cpp')
endif

add_global_arguments(cpp_args, language : 'cpp')
add_global_link_arguments(cpp_link_args, language : 'cpp')

is_linux = host_machine.system() == 'linux'
is_dragonfly = host_machine.system() == 'dragonfly'
is_freebsd = host_machine.system() == 'freebsd'
is_netbsd = host_machine.system() == 'netbsd'
is_openbsd = host_machine.system() == 'openbsd'

thread_dep = dependency('threads')
fmt = dependency('fmt', version : ['>=8.1.1'], fallback : ['fmt', 'fmt_dep'])
spdlog = dependency('spdlog', version : ['>=1.10.0'], fallback : ['spdlog', 'spdlog_dep'], default_options : ['external_fmt=enabled'])
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols')
gtkmm = dependency('gtkmm-3.0', version : ['>=3.22.0'])
dbusmenu_gtk = dependency('dbusmenu-gtk3-0.4', required: get_option('dbusmenu-gtk'))
giounix = dependency('gio-unix-2.0', required: (get_option('dbusmenu-gtk').enabled() or get_option('logind').enabled() or get_option('upower_glib').enabled()))
jsoncpp = dependency('jsoncpp', version : ['>=1.9.2'], fallback : ['jsoncpp', 'jsoncpp_dep'])
sigcpp = dependency('sigc++-2.0')
libinotify = dependency('libinotify', required: false)
libepoll = dependency('epoll-shim', required: false)
libinput = dependency('libinput', required: get_option('libinput'))
libnl = dependency('libnl-3.0', required: get_option('libnl'))
libnlgen = dependency('libnl-genl-3.0', required: get_option('libnl'))
upower_glib = dependency('upower-glib', required: get_option('upower_glib'))
libpulse = dependency('libpulse', required: get_option('pulseaudio'))
libudev = dependency('libudev', required: get_option('libudev'))
libevdev = dependency('libevdev', required: get_option('libevdev'))
libmpdclient = dependency('libmpdclient', required: get_option('mpd'))
xkbregistry = dependency('xkbregistry')
libjack = dependency('jack', required: get_option('jack'))
libwireplumber = dependency('wireplumber-0.4', required: get_option('wireplumber'))

libsndio = compiler.find_library('sndio', required: get_option('sndio'))
if libsndio.found()
    if not compiler.has_function('sioctl_open', prefix: '#include <sndio.h>', dependencies: libsndio)
        if get_option('sndio').enabled()
            error('libsndio is too old, required >=1.7.0')
        else
            warning('libsndio is too old, required >=1.7.0')
            libsndio = dependency('', required: false)
        endif
    endif
endif

gtk_layer_shell = dependency('gtk-layer-shell-0',
        required: get_option('gtk-layer-shell'),
        fallback : ['gtk-layer-shell', 'gtk_layer_shell_dep'])
systemd = dependency('systemd', required: get_option('systemd'))
tz_dep = dependency('date',
    required: false,
    default_options : [ 'use_system_tzdb=true' ],
    modules : [ 'date::date', 'date::date-tz' ],
    fallback: [ 'date', 'tz_dep' ])

prefix = get_option('prefix')
sysconfdir = get_option('sysconfdir')
conf_data = configuration_data()
conf_data.set('prefix', prefix)

add_project_arguments('-DSYSCONFDIR="/@0@"'.format(join_paths(prefix, sysconfdir)), language : 'cpp')

if systemd.found()
  user_units_dir = systemd.get_pkgconfig_variable('systemduserunitdir')

  configure_file(
    configuration: conf_data,
    input: './resources/waybar.service.in',
    output: '@BASENAME@',
    install_dir: user_units_dir
  )
endif

src_files = files(
    'src/factory.cpp',
    'src/AModule.cpp',
    'src/ALabel.cpp',
    'src/AIconLabel.cpp',
    'src/modules/custom.cpp',
    'src/modules/disk.cpp',
    'src/modules/idle_inhibitor.cpp',
    'src/modules/temperature.cpp',
    'src/modules/user.cpp',
    'src/main.cpp',
    'src/bar.cpp',
    'src/client.cpp',
    'src/config.cpp',
    'src/group.cpp',
    'src/util/ustring_clen.cpp',
    'src/util/sanitize_str.cpp',
    'src/util/rewrite_title.cpp'
)

if is_linux
    add_project_arguments('-DHAVE_CPU_LINUX', language: 'cpp')
    add_project_arguments('-DHAVE_MEMORY_LINUX', language: 'cpp')
    src_files += files(
        'src/modules/battery.cpp',
        'src/modules/cpu/common.cpp',
        'src/modules/cpu/linux.cpp',
        'src/modules/memory/common.cpp',
        'src/modules/memory/linux.cpp',
    )
elif is_dragonfly or is_freebsd or is_netbsd or is_openbsd
    add_project_arguments('-DHAVE_CPU_BSD', language: 'cpp')
    add_project_arguments('-DHAVE_MEMORY_BSD', language: 'cpp')
    src_files += files(
        'src/modules/cpu/bsd.cpp',
        'src/modules/cpu/common.cpp',
        'src/modules/memory/bsd.cpp',
        'src/modules/memory/common.cpp',
    )
    if is_freebsd
    src_files += files(
        'src/modules/battery.cpp',
        )
    endif
endif

add_project_arguments('-DHAVE_SWAY', language: 'cpp')
src_files += [
    'src/modules/sway/ipc/client.cpp',
    'src/modules/sway/bar.cpp',
    'src/modules/sway/mode.cpp',
    'src/modules/sway/language.cpp',
    'src/modules/sway/window.cpp',
    'src/modules/sway/workspaces.cpp',
    'src/modules/sway/scratchpad.cpp'
]

if true
    add_project_arguments('-DHAVE_WLR', language: 'cpp')
    src_files += 'src/modules/wlr/taskbar.cpp'
    src_files += 'src/modules/wlr/workspace_manager.cpp'
    src_files += 'src/modules/wlr/workspace_manager_binding.cpp'
endif

if true
    add_project_arguments('-DHAVE_RIVER', language: 'cpp')
    src_files += 'src/modules/river/mode.cpp'
    src_files += 'src/modules/river/tags.cpp'
    src_files += 'src/modules/river/window.cpp'
endif

if true
    add_project_arguments('-DHAVE_HYPRLAND', language: 'cpp')
    src_files += 'src/modules/hyprland/backend.cpp'
    src_files += 'src/modules/hyprland/window.cpp'
    src_files += 'src/modules/hyprland/language.cpp'
endif

if libnl.found() and libnlgen.found()
    add_project_arguments('-DHAVE_LIBNL', language: 'cpp')
    src_files += 'src/modules/network.cpp'
endif

if (giounix.found() and not get_option('logind').disabled())
    add_project_arguments('-DHAVE_GAMEMODE', language: 'cpp')
    src_files += 'src/modules/gamemode.cpp'
endif

if (upower_glib.found() and giounix.found() and not get_option('logind').disabled())
    add_project_arguments('-DHAVE_UPOWER', language: 'cpp')
    src_files += 'src/modules/upower/upower.cpp'
    src_files += 'src/modules/upower/upower_tooltip.cpp'
endif

if libpulse.found()
    add_project_arguments('-DHAVE_LIBPULSE', language: 'cpp')
    src_files += 'src/modules/pulseaudio.cpp'
endif

if libjack.found()
    add_project_arguments('-DHAVE_LIBJACK', language: 'cpp')
    src_files += 'src/modules/jack.cpp'
endif

if libwireplumber.found()
    add_project_arguments('-DHAVE_LIBWIREPLUMBER', language: 'cpp')
    src_files += 'src/modules/wireplumber.cpp'
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

if libudev.found() and (is_linux or libepoll.found())
    add_project_arguments('-DHAVE_LIBUDEV', language: 'cpp')
    src_files += 'src/modules/backlight.cpp'
endif

if libevdev.found() and (is_linux or libepoll.found()) and libinput.found() and (is_linux or libinotify.found())
    add_project_arguments('-DHAVE_LIBEVDEV', language: 'cpp')
    add_project_arguments('-DHAVE_LIBINPUT', language: 'cpp')
    src_files += 'src/modules/keyboard_state.cpp'
endif

if libmpdclient.found()
    add_project_arguments('-DHAVE_LIBMPDCLIENT', language: 'cpp')
    src_files += 'src/modules/mpd/mpd.cpp'
    src_files += 'src/modules/mpd/state.cpp'
endif

if gtk_layer_shell.found()
    add_project_arguments('-DHAVE_GTK_LAYER_SHELL', language: 'cpp')
endif

if libsndio.found()
    add_project_arguments('-DHAVE_LIBSNDIO', language: 'cpp')
    src_files += 'src/modules/sndio.cpp'
endif

if (giounix.found() and not get_option('logind').disabled())
    add_project_arguments('-DHAVE_GIO_UNIX', language: 'cpp')
    src_files += 'src/modules/inhibitor.cpp'
    src_files += 'src/modules/bluetooth.cpp'
endif

if get_option('rfkill').enabled() and is_linux
    add_project_arguments('-DWANT_RFKILL', language: 'cpp')
    src_files += files(
        'src/util/rfkill.cpp'
    )
endif

if tz_dep.found()
    add_project_arguments('-DHAVE_LIBDATE', language: 'cpp')
    src_files += 'src/modules/clock.cpp'
else
    src_files += 'src/modules/simpleclock.cpp'
endif

if get_option('experimental')
    add_project_arguments('-DUSE_EXPERIMENTAL', language: 'cpp')
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
        wayland_cursor,
        gtkmm,
        dbusmenu_gtk,
        giounix,
        libinput,
        libnl,
        libnlgen,
        upower_glib,
        libpulse,
        libjack,
        libwireplumber,
        libudev,
        libinotify,
        libepoll,
        libmpdclient,
        libevdev,
        gtk_layer_shell,
        libsndio,
        tz_dep,
		xkbregistry
    ],
    include_directories: [include_directories('include')],
    install: true,
)

install_data(
    './resources/config',
    './resources/style.css',
    install_dir: sysconfdir + '/xdg/waybar'
)

scdoc = dependency('scdoc', version: '>=1.9.2', native: true, required: get_option('man-pages'))

if scdoc.found()
    scdoc_prog = find_program(scdoc.get_pkgconfig_variable('scdoc'), native: true)
    sh = find_program('sh', native: true)

    main_manpage = configure_file(
        input: 'man/waybar.5.scd.in',
        output: 'waybar.5.scd',
        configuration: {
            'sysconfdir': join_paths(prefix, sysconfdir)
        }
    )

    main_manpage_path = join_paths(meson.build_root(), '@0@'.format(main_manpage))

    mandir = get_option('mandir')
    man_files = [
        main_manpage_path,
        'waybar-backlight.5.scd',
        'waybar-battery.5.scd',
        'waybar-clock.5.scd',
        'waybar-cpu.5.scd',
        'waybar-custom.5.scd',
        'waybar-disk.5.scd',
        'waybar-gamemode.5.scd',
        'waybar-idle-inhibitor.5.scd',
        'waybar-keyboard-state.5.scd',
        'waybar-memory.5.scd',
        'waybar-mpd.5.scd',
        'waybar-network.5.scd',
        'waybar-pulseaudio.5.scd',
        'waybar-river-mode.5.scd',
        'waybar-river-tags.5.scd',
        'waybar-river-window.5.scd',
        'waybar-sway-language.5.scd',
        'waybar-sway-mode.5.scd',
        'waybar-sway-scratchpad.5.scd',
        'waybar-sway-window.5.scd',
        'waybar-sway-workspaces.5.scd',
        'waybar-temperature.5.scd',
        'waybar-tray.5.scd',
        'waybar-states.5.scd',
        'waybar-wlr-taskbar.5.scd',
        'waybar-wlr-workspaces.5.scd',
        'waybar-bluetooth.5.scd',
        'waybar-sndio.5.scd',
        'waybar-upower.5.scd',
    ]

    if (giounix.found() and not get_option('logind').disabled())
        man_files += 'waybar-inhibitor.5.scd'
    endif

    foreach file : man_files
        path = '@0@'.format(file)
        basename = path.split('/')[-1]

        topic = basename.split('.')[-3]
        section = basename.split('.')[-2]
        output = '@0@.@1@'.format(topic, section)

        custom_target(
            output,
            # drops the 'man' if `path` is an absolute path
            input: join_paths('man', path),
            output: output,
            command: [
                sh, '-c', '@0@ < @INPUT@ > @1@'.format(scdoc_prog.path(), output)
            ],
            install: true,
            install_dir: '@0@/man@1@'.format(mandir, section)
        )
    endforeach
endif

catch2 = dependency(
    'catch2',
    version: '>=3.0.0',
    fallback: ['catch2', 'catch2_dep'],
    required: get_option('tests'),
)
if catch2.found()
    subdir('test')
endif

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

