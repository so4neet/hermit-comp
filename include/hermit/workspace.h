#pragma once

#include <wayland-server-core.h>
#include <stdbool.h>

#define HERMIT_MAX_WORKSPACES    10 // SET THIS TO BE SCALED WITH NUMBER OF MONITORS

struct hermit_server;
struct hermit_output;
struct hermit_view;

struct hermit_workspace {
    int index;
    struct hermit_output *output;
    struct wl_list views;
    bool is_active;
};

void hermit_workspaces_init(struct hermit_server *server);
void hermit_workspace_switch(struct hermit_output *output, int index);
void hermit_workspace_move_view(struct hermit_view *view, int index);
struct hermit_workspace *hermit_workspace_get(struct hermit_server *server, int index);
struct hermit_output *hermit_output_for_workspace(struct hermit_server *server, int index);