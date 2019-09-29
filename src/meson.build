version_h = vcs_tag(input: 'version.h.in',
		    output: 'version.h')

libsrc = [
  'chatty-settings.c',
]

chatty_sources = [
  'main.c',
  'chatty-application.c',
  'chatty-window.c',
  'chatty-dialogs.c',
  'chatty-settings-dialog.c',
  'chatty-account.c',
  'chatty-message-list.c',
  'chatty-contact-row.c',
  'chatty-conversation.c',
  'chatty-connection.c',
  'chatty-buddy-list.c',
  './xeps/xeps.c',
  './xeps/chatty-xep-0184.c',
  'chatty-purple-init.c',
  'chatty-icons.c',
  'chatty-history.c',
  'chatty-dbus.c',
  'chatty-lurch.c',
  'chatty-utils.c',
  'chatty-folks.c',
  'chatty-notify.c',
  'chatty-purple-request.c',
  'chatty-purple-notify.c',
  'chatty-popover-actions.c',
  version_h,
]

purple = dependency('purple')
purple_plugdir = purple.get_pkgconfig_variable('plugindir')
jabber = meson.get_compiler('c').find_library('jabber', dirs: purple_plugdir)
jabber_incdir = include_directories('xeps/prpl/jabber')

chatty_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  purple, jabber,
  dependency('libhandy-0.0'),
  dependency('sqlite3', version: '>=3.0.0'),
  dependency('gee-0.8'),
  dependency('folks'),
  libebook_dep,
  libm_dep,
]

gnome = import('gnome')

chatty_sources += gnome.compile_resources('chatty-resources',
  'chatty.gresource.xml',
  c_name: 'chatty'
)

libchatty = static_library(
  'libchatty', libsrc,
  install: false,
  dependencies: chatty_deps,
)

executable('chatty', chatty_sources,
  include_directories: jabber_incdir,
  dependencies: chatty_deps,
  link_with: libchatty,
  install: true,
)
