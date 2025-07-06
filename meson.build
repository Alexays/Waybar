project(
    'waybar', 'cpp', 'c',
    version: '0.13.0',
    license: 'MIT',
    meson_version: '>= 0.59.0',
    default_options : [
        'cpp_std=c++20',
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
elif compiler.has_link_argument('-lstdc++fs')
    cpp_link_args += ['-lstdc++fs']
endif

git = find_program('git', native: true, required: false)

if not git.found()
    add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'cpp')
else
    git_path = run_command(git, 'rev-parse', '--show-toplevel', check: false).stdout().strip()
    if meson.project_source_root() == git_path
        git_commit_hash = run_command(git, 'describe', '--always', '--tags', check: false).stdout().strip()
        git_branch = run_command(git, 'rev-parse', '--abbrev-ref', 'HEAD', check: false).stdout().strip()
        version = '"@0@ (branch \'@1@\')"'.format(git_commit_hash, git_branch)
        add_project_arguments('-DVERSION=@0@'.format(version), language: 'cpp')
    else
        add_project_arguments('-DVERSION="@0@"'.format(meson.project_version()), language: 'cpp')
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
spdlog = dependency('spdlog', version : ['>=1.10.0'], fallback : ['spdlog', 'spdlog_dep'], default_options : ['external_fmt=enabled', 'std_format=disabled', 'tests=disabled'])
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols')
gtkmm = dependency('gtkmm-3.0', version : ['>=3.22.0'])
dbusmenu_gtk = dependency('dbusmenu-gtk3-0.4', required: get_option('dbusmenu-gtk'))
giounix = dependency('gio-unix-2.0')
jsoncpp = dependency('jsoncpp', version : ['>=1.9.2'], fallback : ['jsoncpp', 'jsoncpp_dep'])
sigcpp = dependency('sigc++-2.0')
libinotify = dependency('libinotify', required: false)
libepoll = dependency('epoll-shim', required: false)
libinput = dependency('libinput', required: get_option('libinput'))
libnl = dependency('libnl-3.0', required: get_option('libnl'))
libnlgen = dependency('libnl-genl-3.0', required: get_option('libnl'))
upower_glib = dependency('upower-glib', required: get_option('upower_glib'))
pipewire = dependency('libpipewire-0.3', required: get_option('pipewire'))
playerctl = dependency('playerctl', version : ['>=2.0.0'], required: get_option('mpris'))
libpulse = dependency('libpulse', required: get_option('pulseaudio'))
libudev = dependency('libudev', required: get_option('libudev'))
libevdev = dependency('libevdev', required: get_option('libevdev'))
libmpdclient = dependency('libmpdclient', required: get_option('mpd'))
xkbregistry = dependency('xkbregistry')
libjack = dependency('jack', required: get_option('jack'))
libwireplumber = dependency('wireplumber-0.5', required: get_option('wireplumber'))
libgps = dependency('libgps', required: get_option('gps'))

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

gtk_layer_shell = dependency('gtk-layer-shell-0', version: ['>=0.9.0'],
        default_options: ['introspection=false', 'vapi=false'],
        fallback: ['gtk-layer-shell', 'gtk_layer_shell'])
systemd = dependency('systemd', required: get_option('systemd'))

cpp_lib_chrono = compiler.compute_int('__cpp_lib_chrono', prefix : '#include <chrono>')
have_chrono_timezones = cpp_lib_chrono >= 201611

if have_chrono_timezones
   code = '''
#include <chrono>
using namespace std::chrono;
int main(int argc, char** argv) {
   const time_zone* tz;
   return 0;
}
'''
   if not compiler.links(code)
      have_chrono_timezones = false
   endif
endif

if have_chrono_timezones
  tz_dep = declare_dependency()
else
  tz_dep = dependency('date',
      required: false,
      default_options : [ 'use_system_tzdb=true' ],
      modules : [ 'date::date', 'date::date-tz' ],
      fallback: [ 'date', 'tz_dep' ])
endif

prefix = get_option('prefix')
sysconfdir = get_option('sysconfdir')
conf_data = configuration_data()
conf_data.set('prefix', prefix)

add_project_arguments('-DSYSCONFDIR="@0@"'.format(prefix / sysconfdir), language : 'cpp')

if systemd.found()
  user_units_dir = systemd.get_variable(pkgconfig: 'systemduserunitdir')

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
    'src/AAppIconLabel.cpp',
    'src/modules/custom.cpp',
    'src/modules/disk.cpp',
    'src/modules/idle_inhibitor.cpp',
    'src/modules/image.cpp',
    'src/modules/load.cpp',
    'src/modules/temperature.cpp',
    'src/modules/user.cpp',
    'src/ASlider.cpp',
    'src/main.cpp',
    'src/bar.cpp',
    'src/client.cpp',
    'src/config.cpp',
    'src/group.cpp',
    'src/util/portal.cpp',
    'src/util/enum.cpp',
    'src/util/prepare_for_sleep.cpp',
    'src/util/ustring_clen.cpp',
    'src/util/sanitize_str.cpp',
    'src/util/rewrite_string.cpp',
    'src/util/gtk_icon.cpp',
    'src/util/icon_loader.cpp',
    'src/util/regex_collection.cpp',
    'src/util/css_reload_helper.cpp'
)

man_files = files(
    'man/waybar-custom.5.scd',
    'man/waybar-disk.5.scd',
    'man/waybar-idle-inhibitor.5.scd',
    'man/waybar-image.5.scd',
    'man/waybar-states.5.scd',
    'man/waybar-menu.5.scd',
    'man/waybar-temperature.5.scd',
)

inc_dirs = ['include']

if is_linux
    add_project_arguments('-DHAVE_CPU_LINUX', language: 'cpp')
    add_project_arguments('-DHAVE_MEMORY_LINUX', language: 'cpp')
    add_project_arguments('-DHAVE_SYSTEMD_MONITOR', language: 'cpp')
    src_files += files(
        'src/modules/battery.cpp',
        'src/modules/bluetooth.cpp',
        'src/modules/cffi.cpp',
        'src/modules/cpu.cpp',
        'src/modules/cpu_frequency/common.cpp',
        'src/modules/cpu_frequency/linux.cpp',
        'src/modules/cpu_usage/common.cpp',
        'src/modules/cpu_usage/linux.cpp',
        'src/modules/memory/common.cpp',
        'src/modules/memory/linux.cpp',
        'src/modules/power_profiles_daemon.cpp',
        'src/modules/systemd_failed_units.cpp',
    )
    man_files += files(
        'man/waybar-battery.5.scd',
        'man/waybar-bluetooth.5.scd',
        'man/waybar-cffi.5.scd',
        'man/waybar-cpu.5.scd',
        'man/waybar-memory.5.scd',
        'man/waybar-systemd-failed-units.5.scd',
        'man/waybar-power-profiles-daemon.5.scd',
    )
elif is_dragonfly or is_freebsd or is_netbsd or is_openbsd
    add_project_arguments('-DHAVE_CPU_BSD', language: 'cpp')
    add_project_arguments('-DHAVE_MEMORY_BSD', language: 'cpp')
    src_files += files(
        'src/modules/cffi.cpp',
        'src/modules/cpu.cpp',
        'src/modules/cpu_frequency/bsd.cpp',
        'src/modules/cpu_frequency/common.cpp',
        'src/modules/cpu_usage/bsd.cpp',
        'src/modules/cpu_usage/common.cpp',
        'src/modules/memory/bsd.cpp',
        'src/modules/memory/common.cpp',
    )
    man_files += files(
        'man/waybar-cffi.5.scd',
        'man/waybar-cpu.5.scd',
        'man/waybar-memory.5.scd',
    )
    if is_freebsd
        src_files += files('src/modules/battery.cpp')
        man_files += files('man/waybar-battery.5.scd')
    endif
endif

if true
    add_project_arguments('-DHAVE_SWAY', language: 'cpp')
    src_files += files(
        'src/modules/sway/ipc/client.cpp',
        'src/modules/sway/bar.cpp',
        'src/modules/sway/mode.cpp',
        'src/modules/sway/language.cpp',
        'src/modules/sway/window.cpp',
        'src/modules/sway/workspaces.cpp',
        'src/modules/sway/scratchpad.cpp'
    )
    man_files += files(
        'man/waybar-sway-language.5.scd',
        'man/waybar-sway-mode.5.scd',
        'man/waybar-sway-scratchpad.5.scd',
        'man/waybar-sway-window.5.scd',
        'man/waybar-sway-workspaces.5.scd',
    )
endif

if true
    add_project_arguments('-DHAVE_WLR_TASKBAR', language: 'cpp')
    src_files += files('src/modules/wlr/taskbar.cpp')
    man_files += files('man/waybar-wlr-taskbar.5.scd')
endif

if true
    add_project_arguments('-DHAVE_RIVER', language: 'cpp')
    src_files += files(
        'src/modules/river/layout.cpp',
        'src/modules/river/mode.cpp',
        'src/modules/river/tags.cpp',
        'src/modules/river/window.cpp',
    )
    man_files += files(
        'man/waybar-river-layout.5.scd',
        'man/waybar-river-mode.5.scd',
        'man/waybar-river-tags.5.scd',
        'man/waybar-river-window.5.scd',
    )
endif

if true
    add_project_arguments('-DHAVE_DWL', language: 'cpp')
    src_files += files('src/modules/dwl/tags.cpp')
    src_files += files('src/modules/dwl/window.cpp')
    man_files += files('man/waybar-dwl-tags.5.scd')
    man_files += files('man/waybar-dwl-window.5.scd')
endif

if true
    add_project_arguments('-DHAVE_HYPRLAND', language: 'cpp')
    src_files += files(
        'src/modules/hyprland/backend.cpp',
        'src/modules/hyprland/language.cpp',
        'src/modules/hyprland/submap.cpp',
        'src/modules/hyprland/window.cpp',
        'src/modules/hyprland/windowcount.cpp',
        'src/modules/hyprland/workspace.cpp',
        'src/modules/hyprland/workspaces.cpp',
        'src/modules/hyprland/windowcreationpayload.cpp',
    )
    man_files += files(
        'man/waybar-hyprland-language.5.scd',
        'man/waybar-hyprland-submap.5.scd',
        'man/waybar-hyprland-window.5.scd',
        'man/waybar-hyprland-workspaces.5.scd',
    )
endif

if get_option('niri')
    add_project_arguments('-DHAVE_NIRI', language: 'cpp')
    src_files += files(
        'src/modules/niri/backend.cpp',
        'src/modules/niri/language.cpp',
        'src/modules/niri/window.cpp',
        'src/modules/niri/workspaces.cpp',
    )
    man_files += files(
        'man/waybar-niri-language.5.scd',
        'man/waybar-niri-window.5.scd',
        'man/waybar-niri-workspaces.5.scd',
    )
endif

if true
    add_project_arguments('-DHAVE_WAYFIRE', language: 'cpp')
    src_files += files(
        'src/modules/wayfire/backend.cpp',
        'src/modules/wayfire/window.cpp',
        'src/modules/wayfire/workspaces.cpp',
    )
endif

if get_option('login-proxy')
    add_project_arguments('-DHAVE_LOGIN_PROXY', language: 'cpp')
endif

if libnl.found() and libnlgen.found()
    add_project_arguments('-DHAVE_LIBNL', language: 'cpp')
    src_files += files('src/modules/network.cpp')
    man_files += files('man/waybar-network.5.scd')
endif

if not get_option('logind').disabled()
    add_project_arguments('-DHAVE_GAMEMODE', '-DHAVE_LOGIND_INHIBITOR', language: 'cpp')
    src_files += files(
        'src/modules/gamemode.cpp',
        'src/modules/inhibitor.cpp',
    )
    man_files += files(
        'man/waybar-gamemode.5.scd',
        'man/waybar-inhibitor.5.scd',
    )
endif

if (upower_glib.found() and not get_option('logind').disabled())
    add_project_arguments('-DHAVE_UPOWER', language: 'cpp')
    src_files += files('src/modules/upower.cpp')
    man_files += files('man/waybar-upower.5.scd')
endif


if pipewire.found()
    add_project_arguments('-DHAVE_PIPEWIRE', language: 'cpp')
    src_files += files(
        'src/modules/privacy/privacy.cpp',
        'src/modules/privacy/privacy_item.cpp',
        'src/util/pipewire/pipewire_backend.cpp',
        'src/util/pipewire/privacy_node_info.cpp',
    )
    man_files += files('man/waybar-privacy.5.scd')
endif

if playerctl.found()
    add_project_arguments('-DHAVE_MPRIS', language: 'cpp')
    src_files += files('src/modules/mpris/mpris.cpp')
    man_files += files('man/waybar-mpris.5.scd')
endif

if libpulse.found()
    add_project_arguments('-DHAVE_LIBPULSE', language: 'cpp')
    src_files += files(
        'src/modules/pulseaudio.cpp',
        'src/modules/pulseaudio_slider.cpp',
        'src/util/audio_backend.cpp',
    )
    man_files += files(
        'man/waybar-pulseaudio.5.scd',
        'man/waybar-pulseaudio-slider.5.scd',
    )
endif

if libjack.found()
    add_project_arguments('-DHAVE_LIBJACK', language: 'cpp')
    src_files += files('src/modules/jack.cpp')
    man_files += files('man/waybar-jack.5.scd')
endif

if libwireplumber.found()
    add_project_arguments('-DHAVE_LIBWIREPLUMBER', language: 'cpp')
    src_files += files('src/modules/wireplumber.cpp')
    man_files += files('man/waybar-wireplumber.5.scd')
endif

if dbusmenu_gtk.found()
    add_project_arguments('-DHAVE_DBUSMENU', language: 'cpp')
    src_files += files(
        'src/modules/sni/tray.cpp',
        'src/modules/sni/watcher.cpp',
        'src/modules/sni/host.cpp',
        'src/modules/sni/item.cpp'
    )
    man_files += files(
        'man/waybar-tray.5.scd',
    )
endif

if libudev.found() and (is_linux or libepoll.found())
    add_project_arguments('-DHAVE_LIBUDEV', language: 'cpp')
    src_files += files(
        'src/modules/backlight.cpp',
        'src/modules/backlight_slider.cpp',
        'src/util/backlight_backend.cpp',
    )
    man_files += files(
        'man/waybar-backlight.5.scd',
        'man/waybar-backlight-slider.5.scd',
    )
endif

if libevdev.found() and (is_linux or libepoll.found()) and libinput.found() and (is_linux or libinotify.found())
    add_project_arguments('-DHAVE_LIBEVDEV', language: 'cpp')
    add_project_arguments('-DHAVE_LIBINPUT', language: 'cpp')
    src_files += files('src/modules/keyboard_state.cpp')
    man_files += files('man/waybar-keyboard-state.5.scd')
endif

if libmpdclient.found()
    add_project_arguments('-DHAVE_LIBMPDCLIENT', language: 'cpp')
    src_files += files(
        'src/modules/mpd/mpd.cpp',
        'src/modules/mpd/state.cpp',
    )
    man_files += files(
        'man/waybar-mpd.5.scd',
    )
endif

if libsndio.found()
    add_project_arguments('-DHAVE_LIBSNDIO', language: 'cpp')
    src_files += files('src/modules/sndio.cpp')
    man_files += files('man/waybar-sndio.5.scd')
endif

if get_option('rfkill').enabled() and is_linux
    add_project_arguments('-DWANT_RFKILL', language: 'cpp')
    src_files += files(
        'src/util/rfkill.cpp'
    )
endif

if have_chrono_timezones
    add_project_arguments('-DHAVE_CHRONO_TIMEZONES', language: 'cpp')
    src_files += files('src/modules/clock.cpp')
    man_files += files('man/waybar-clock.5.scd')
elif tz_dep.found()
    add_project_arguments('-DHAVE_LIBDATE', language: 'cpp')
    src_files += files('src/modules/clock.cpp')
    man_files += files('man/waybar-clock.5.scd')
else
    src_files += files('src/modules/simpleclock.cpp')
    man_files += files('man/waybar-clock.5.scd')
endif

if get_option('experimental')
    add_project_arguments('-DHAVE_WLR_WORKSPACES', language: 'cpp')
    src_files += files(
        'src/modules/wlr/workspace_manager.cpp',
        'src/modules/wlr/workspace_manager_binding.cpp',
    )
    man_files += files(
        'man/waybar-wlr-workspaces.5.scd',
    )
endif

cava = dependency('cava',
                  version : '>=0.10.4',
                  required: get_option('cava'),
                  fallback : ['cava', 'cava_dep'],
                  not_found_message: 'cava is not found. Building waybar without cava')

if cava.found()
   add_project_arguments('-DHAVE_LIBCAVA', language: 'cpp')
   src_files += files('src/modules/cava.cpp')
   man_files += files('man/waybar-cava.5.scd')
endif

if libgps.found()
   add_project_arguments('-DHAVE_LIBGPS', language: 'cpp')
   src_files += files('src/modules/gps.cpp')
   man_files += files('man/waybar-gps.5.scd')
endif

subdir('protocol')

app_resources = []
subdir('resources/icons')

executable(
    'waybar',
    [src_files, app_resources],
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
        pipewire,
        playerctl,
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
		xkbregistry,
        cava,
        libgps
    ],
    include_directories: inc_dirs,
    install: true,
)

install_data(
    'resources/config.jsonc',
    'resources/style.css',
    install_dir: sysconfdir / 'xdg/waybar'
)

scdoc = dependency('scdoc', version: '>=1.9.2', native: true, required: get_option('man-pages'))

if scdoc.found()
    man_files += configure_file(
        input: 'man/waybar.5.scd.in',
        output: 'waybar.5.scd',
        configuration: {
            'sysconfdir': prefix / sysconfdir
        }
    )

    man_files += configure_file(
        input: 'man/waybar-styles.5.scd.in',
        output: 'waybar-styles.5.scd',
        configuration: {
            'sysconfdir': prefix / sysconfdir
        }
    )

    fs = import('fs')
    mandir = get_option('mandir')
    foreach file : man_files
        basename = fs.name(file)

        topic = basename.split('.')[-3]
        section = basename.split('.')[-2]
        output = '@0@.@1@'.format(topic, section)

        custom_target(
            output,
            input: file,
            output: output,
            command: scdoc.get_variable('scdoc'),
            feed: true,
            capture: true,
            install: true,
            install_dir: '@0@/man@1@'.format(mandir, section)
        )
    endforeach
endif

catch2 = dependency(
    'catch2',
    default_options: [ 'tests=false' ],
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
            '-p', meson.project_build_root()
        ] + src_files)
endif
