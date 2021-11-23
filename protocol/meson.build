wl_protocol_dir = wayland_protos.get_pkgconfig_variable('pkgdatadir')

wayland_scanner = find_program('wayland-scanner')

# should check wayland_scanner's version, but it is hard to get
if wayland_client.version().version_compare('>=1.14.91')
	code_type = 'private-code'
else
	code_type = 'code'
endif

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: [code_type, '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

client_protocols = [
	[wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
	[wl_protocol_dir, 'unstable/xdg-output/xdg-output-unstable-v1.xml'],
	[wl_protocol_dir, 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml'],
	['wlr-layer-shell-unstable-v1.xml'],
	['wlr-foreign-toplevel-management-unstable-v1.xml'],
	['ext-workspace-unstable-v1.xml'],
	['river-status-unstable-v1.xml'],
	['river-control-unstable-v1.xml'],
]

client_protos_src = []
client_protos_headers = []

foreach p : client_protocols
	xml = join_paths(p)
	client_protos_src += wayland_scanner_code.process(xml)
	client_protos_headers += wayland_scanner_client.process(xml)
endforeach

gdbus_codegen = find_program('gdbus-codegen')

r = run_command(gdbus_codegen, '--body', '--output', '/dev/null')
if r.returncode() != 0
  gdbus_code_dsnw = custom_target(
      'dbus-status-notifier-watcher.[ch]',
      output: ['@BASENAME@.c','@BASENAME@.h'],
      input: './dbus-status-notifier-watcher.xml',
      command: [gdbus_codegen,'--c-namespace', 'Sn', '--generate-c-code', 'protocol/@BASENAME@', '@INPUT@'],
  )

  gdbus_code_dsni = custom_target(
      'dbus-status-notifier-item.[ch]',
      output: ['@BASENAME@.c','@BASENAME@.h'],
      input: './dbus-status-notifier-item.xml',
      command: [gdbus_codegen,'--c-namespace', 'Sn', '--generate-c-code', 'protocol/@BASENAME@', '@INPUT@'],
  )

  gdbus_code_dm = custom_target(
      'dbus-menu.[ch]',
      output: ['@BASENAME@.c','@BASENAME@.h'],
      input: './dbus-menu.xml',
      command: [gdbus_codegen,'--c-namespace', 'Sn', '--generate-c-code', 'protocol/@BASENAME@', '@INPUT@'],
  )

  client_protos_src += gdbus_code_dsnw[0]
  client_protos_headers += gdbus_code_dsnw[1]
  client_protos_src += gdbus_code_dsni[0]
  client_protos_headers += gdbus_code_dsni[1]
  client_protos_src += gdbus_code_dm[0]
  client_protos_headers += gdbus_code_dm[1]
else
  gdbus_code = generator(
      gdbus_codegen,
      output: '@BASENAME@.c',
      arguments: ['--c-namespace', 'Sn', '--body', '--output', '@OUTPUT@', '@INPUT@']
  )

  gdbus_header = generator(
      gdbus_codegen,
      output: '@BASENAME@.h',
      arguments: ['--c-namespace', 'Sn', '--header', '--output', '@OUTPUT@', '@INPUT@']
  )

  client_protos_src += gdbus_code.process('./dbus-status-notifier-watcher.xml')
  client_protos_headers += gdbus_header.process('./dbus-status-notifier-watcher.xml')

  client_protos_src += gdbus_code.process('./dbus-status-notifier-item.xml')
  client_protos_headers += gdbus_header.process('./dbus-status-notifier-item.xml')

  client_protos_src += gdbus_code.process('./dbus-menu.xml')
  client_protos_headers += gdbus_header.process('./dbus-menu.xml')
endif


lib_client_protos = static_library(
	'client_protos',
	client_protos_src + client_protos_headers,
	dependencies: [wayland_client, gtkmm, giounix],
	include_directories: include_directories('..'),
) # for the include directory

client_protos = declare_dependency(
	link_with: lib_client_protos,
	sources: client_protos_headers,
)
