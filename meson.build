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

jsoncpp = dependency('jsoncpp', version : ['>=1.9.4'], fallback : ['jsoncpp', 'jsoncpp_dep'])
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols')
sigcpp = dependency('sigc++-3.0', version: ['>=3.4.0'])
gtkmm = dependency('gtkmm-4.0', version : ['>=4.12.0'])
giounix = dependency('gio-unix-2.0', version: ['>=2.76.4'])
spdlog = dependency('spdlog', version : ['>=1.10.0'], fallback : ['spdlog', 'spdlog_dep'], default_options : ['external_fmt=enabled'])

gtk_layer_shell = dependency('gtk4-layer-shell-0',
                             version : ['>=1.0.2'],
                             required: get_option('gtk-layer-shell'),
                             fallback : ['gtk4-layer-shell', 'gtk_layer_shell'],
                             default_options : ['vapi=false'])

if gtk_layer_shell.found()
    add_project_arguments('-DHAVE_GTK_LAYER_SHELL', language: 'cpp')
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
#    'src/AModule.cpp',
)

inc_dirs = ['include']
subdir('protocol')

executable(
    'waybar',
    [src_files],
    dependencies: [
        gtk_layer_shell,
        gtkmm,
        giounix,
        sigcpp,
        jsoncpp,
        wayland_client,
        wayland_cursor,
        client_protos,
        spdlog
    ],
    include_directories: inc_dirs,
    install: true,
)

install_data(
    './resources/config',
    './resources/style.css',
    install_dir: sysconfdir + '/xdg/waybar'
)
