#pragma once

#include <wayland-server-core.h>
#include <stdbool.h>
#include <hermit/bsp.h>

struct hermit_server;
struct hermit_output;
struct hermit_view;

struct hermit_workspace {
    int index;
    struct hermit_output *output;
    struct wl_list views;
    bool is_active;
    struct hermit_bsp_node *bsp_root;
};

void hermit_workspaces_init(struct hermit_server *server);
void hermit_workspace_switch(struct hermit_output *output, int index);
void hermit_workspace_move_view(struct hermit_view *view, int index);