chatty_sources = [
  'main.c',
  'chatty-window.c',
  'chatty-account.c',
  'chatty-message-list.c',
  'chatty-conversation.c',
  'chatty-connection.c',
  'chatty-buddy-list.c',
  'chatty-purple-init.c',
  'chatty-icons.c',
  'chatty-popover-actions.c'
]

chatty_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  dependency('purple'),
  dependency('libhandy-0.0')
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
