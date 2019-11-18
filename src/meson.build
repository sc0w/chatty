version_h = vcs_tag(command: ['git', 'rev-parse', 'HEAD'],
		    input: 'version.h.in',
		    output: 'version.h')

chatty_sources = [
  'main.c',
  'chatty-application.c',
  'chatty-window.c',
  'chatty-dialogs.c',
  'chatty-account.c',
  'chatty-message-list.c',
  'chatty-contact-row.c',
  'chatty-conversation.c',
  'chatty-connection.c',
  'chatty-buddy-list.c',
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

chatty_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  dependency('purple'),
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

executable('chatty', chatty_sources,
  dependencies: chatty_deps,
  install: true,
)
