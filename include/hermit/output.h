#pragma once 

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>

struct hermit_server;

struct hermit_output {
    struct wl_list       link;
    struct hermit_server *server;
    struct wlr_output    *wlr_output;
    struct wl_listener   frame;
    struct wl_listener   request_state;
    struct wl_listener   destroy;
};

void hermit_output_manager_init(struct hermit_server *server);