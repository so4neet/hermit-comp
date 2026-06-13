#pragma once

#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

struct hermit_server;

struct hermit_view {
    struct wl_list           link;
    struct hermit_server     *server;
    struct wlr_xdg_toplevel  *xdg_toplevel;
    struct wlr_scene_tree    *scene_tree;
    struct hermit_workspace  *workspace;
    struct hermit_bsp_node   *bsp_node;
    struct wlr_box           float_box;
    bool                     initial_configure_sent;
    
    struct wl_listener       commit;
    struct wl_listener       map;
    struct wl_listener       unmap;
    struct wl_listener       destroy;
    struct wl_listener       request_move;
    struct wl_listener       request_resize;
    struct wl_listener       request_maximize;
    struct wl_listener       request_fullscreen;
};

void hermit_view_manager_init(struct hermit_server *server);