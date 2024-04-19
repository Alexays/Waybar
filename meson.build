project(
    'waybar', 'cpp', 'c',
    version: '4.1.0',
    license: 'MIT',
    meson_version: '>= 1.3.0',
    default_options : [
        'cpp_std=c++23',
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

jsoncpp = dependency('jsoncpp', version : ['>=1.9.4'], fallback : ['jsoncpp', 'jsoncpp_dep'])
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols')
sigcpp = dependency('sigc++-3.0', version: ['>=3.4.0'])
gtkmm = dependency('gtkmm-4.0', version : ['>=4.12.0'])
giounix = dependency('gio-unix-2.0', version: ['>=2.76.4'])
spdlog = dependency('spdlog', version : ['>=1.10.0'], fallback : ['spdlog', 'spdlog_dep'], default_options : ['external_fmt=enabled'])
libinotify = dependency('libinotify', required: false)
libepoll = dependency('epoll-shim', required: false)
libinput = dependency('libinput', required: get_option('libinput'))
libudev = dependency('libudev', required: get_option('libudev'))
libevdev = dependency('libevdev', required: get_option('libevdev'))
xkbregistry = dependency('xkbregistry')
libnl = dependency('libnl-3.0', required: get_option('libnl'))
libnlgen = dependency('libnl-genl-3.0', required: get_option('libnl'))
libpulse = dependency('libpulse', required: get_option('pulseaudio'))
upower_glib = dependency('upower-glib', required: get_option('upower_glib'))

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

gtk_layer_shell = dependency('gtk4-layer-shell-0',
                             version : ['>=1.0.2'],
                             fallback : ['gtk4-layer-shell', 'gtk_layer_shell'],
                             default_options : ['introspection=false', 'vapi=false'])

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

add_project_arguments('-DSYSCONFDIR="/@0@"'.format(join_paths(prefix, sysconfdir)), language : 'cpp')

src_files = files(
     'src/main.cpp',
     'src/client.cpp',
     'src/bar.cpp',
     'src/config.cpp',
     'src/util/portal.cpp',
     'src/util/prepare_for_sleep.cpp',
     'src/AModule.cpp',
     'src/ALabel.cpp',
     'src/AIconLabel.cpp',
     'src/AAppIconLabel.cpp',
     'src/factory.cpp',
     'src/util/ustring_clen.cpp',
     'src/group.cpp',
     'src/ASlider.cpp',
     'src/modules/custom.cpp',
     'src/modules/load.cpp',
     'src/modules/disk.cpp',
     'src/modules/temperature.cpp',
     'src/modules/idle_inhibitor.cpp',
     'src/modules/image.cpp',
     'src/util/rewrite_string.cpp'
)

man_files = files(
    'man/waybar-custom.5.scd',
    'man/waybar-disk.5.scd',
    'man/waybar-idle-inhibitor.5.scd',
    'man/waybar-image.5.scd',
    'man/waybar-states.5.scd',
    'man/waybar-temperature.5.scd',
)

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
    add_project_arguments('-DUSE_EXPERIMENTAL', language: 'cpp')
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

cava = dependency('cava',
                  version : '>=0.10.1',
                  required: get_option('cava'),
                  fallback : ['cava', 'cava_dep'],
                  not_found_message: 'cava is not found. Building waybar without cava')

if cava.found()
   add_project_arguments('-DHAVE_LIBCAVA', language: 'cpp')
   src_files += files('src/modules/cava.cpp')
   man_files += files('man/waybar-cava.5.scd')
endif

inc_dirs = ['include']

if is_linux
   add_project_arguments('-DHAVE_CPU_LINUX', language: 'cpp')
   add_project_arguments('-DHAVE_MEMORY_LINUX', language: 'cpp')
   add_project_arguments('-DHAVE_SYSTEMD_MONITOR', language: 'cpp')
   src_files += files('src/modules/battery.cpp',
                      'src/modules/cpu.cpp',
                      'src/modules/cpu_frequency/common.cpp',
                      'src/modules/cpu_frequency/linux.cpp',
                      'src/modules/cpu_usage/common.cpp',
                      'src/modules/cpu_usage/linux.cpp',
                      'src/modules/memory/common.cpp',
                      'src/modules/memory/linux.cpp',
                      'src/modules/bluetooth.cpp')
   man_files += files('man/waybar-battery.5.scd',
                      'man/waybar-bluetooth.5.scd',
                      'man/waybar-cpu.5.scd',
                      'man/waybar-memory.5.scd')
elif is_dragonfly or is_freebsd or is_netbsd or is_openbsd
    add_project_arguments('-DHAVE_CPU_BSD', language: 'cpp')
    add_project_arguments('-DHAVE_MEMORY_BSD', language: 'cpp')
    src_files += files(
#        'src/modules/cffi.cpp',
        'src/modules/cpu.cpp',
        'src/modules/cpu_frequency/bsd.cpp',
        'src/modules/cpu_frequency/common.cpp',
        'src/modules/cpu_usage/bsd.cpp',
        'src/modules/cpu_usage/common.cpp',
        'src/modules/memory/bsd.cpp',
        'src/modules/memory/common.cpp',
    )
    man_files += files(
#        'man/waybar-cffi.5.scd',
        'man/waybar-cpu.5.scd',
        'man/waybar-memory.5.scd'
    )
    if is_freebsd
        src_files += files('src/modules/battery.cpp')
        man_files += files('man/waybar-battery.5.scd')
    endif
endif

add_project_arguments('-DHAVE_SWAY', language: 'cpp')
src_files += files('src/modules/sway/ipc/client.cpp',
                   'src/modules/sway/bar.cpp',
                   'src/modules/sway/mode.cpp',
                   'src/modules/sway/language.cpp',
                   'src/modules/sway/window.cpp',
                   'src/modules/sway/workspaces.cpp',
                   'src/modules/sway/scratchpad.cpp')

man_files += files('man/waybar-sway-language.5.scd',
                   'man/waybar-sway-mode.5.scd',
                   'man/waybar-sway-scratchpad.5.scd',
                   'man/waybar-sway-window.5.scd',
                   'man/waybar-sway-workspaces.5.scd')

add_project_arguments('-DHAVE_DWL', language: 'cpp')
src_files += files('src/modules/dwl/tags.cpp')
man_files += files('man/waybar-dwl-tags.5.scd')

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
    src_files += files(
         'src/modules/upower.cpp'
#        'src/modules/upower/upower.cpp',
#        'src/modules/upower/upower_tooltip.cpp',
    )
    man_files += files('man/waybar-upower.5.scd')
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

subdir('protocol')

executable(
    'waybar',
    [src_files],
    dependencies: [
        gtk_layer_shell,
        gtkmm,
        giounix,
        sigcpp,
        libevdev,
        libudev,
        jsoncpp,
        wayland_client,
        wayland_cursor,
        client_protos,
        spdlog,
        xkbregistry,
        cava,
        libinotify,
        libepoll,
        libinput,
        libnl,
        libnlgen,
        libpulse,
        libsndio,
        upower_glib
    ],
    include_directories: inc_dirs,
    install: true,
)

install_data(
    './resources/config',
    './resources/style.css',
    install_dir: sysconfdir + '/xdg/waybar'
)
