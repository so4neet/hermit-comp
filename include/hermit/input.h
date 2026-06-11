#pragma once

#include <wayland-server-core.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>

struct hermit_server;

struct hermit_keyboard {
    struct wl_list        link;
    struct hermit_server  *server;
    struct wlr_keyboard   *wlr_keyboard;
    struct wl_listener    modifiers;
    struct wl_listener    key;
    struct wl_listener    destroy;
};

void hermit_input_manager_init(struct hermit_server *server);