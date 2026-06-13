project('hermit-comp', 'c', version: '0.1.0', default_options: ['c_std=c17'])

wlroots = dependency('wlroots-0.20')
wayland_server = dependency('wayland-server')
xkbcommon = dependency('xkbcommon')
pixman = dependency('pixman-1')

sources = files(
    'src/main.c',
    'src/server.c',
    'src/output.c',
    'src/view.c',
    'src/input.c',
    'src/config.c',
    'src/workspace.c',
    'src/logger.c'
)

executable('hermit-comp',
    sources,
    include_directories: include_directories('include'),
    dependencies: [wlroots, wayland_server, xkbcommon, pixman],
    c_args: ['-DWLR_USE_UNSTABLE'])