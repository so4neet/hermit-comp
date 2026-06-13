#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_scene.h>
#include <hermit/workspace.h>
#include <hermit/server.h>
#include <hermit/output.h>
#include <hermit/view.h>
#include <hermit/config.h>

void hermit_workspaces_init(struct hermit_server *server) {
    struct hermit_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        // This is hardcoded at 10 right now, but i want this to be set in the config file
        output->workspace_count = 10;
        for (int i=0; i<10; i++){
            struct hermit_workspace *ws = &output->workspaces[i];
            ws->index    = i+1;
            ws->output   = output;
            ws->is_active= (i == 0);
            wl_list_init(&ws->views);
        }
        output->active_workspace = &output->workspaces[0];
    }
}

void hermit_workspace_switch(struct hermit_output *output, int index) {
    struct hermit_workspace *target = NULL;
    for (int i=0; i<output->workspace_count; i++) {
        if (output->workspaces[i].index == index) {
            target = &output->workspaces[i];
            break;
        }
    }
    
    if (!target || target == output->active_workspace) return;
    
    struct hermit_view *view;

    wl_list_for_each(view, &output->active_workspace->views, link)
        wlr_scene_node_set_enabled(&view->scene_tree->node, false);
    output->active_workspace->is_active = false;
    
    wl_list_for_each(view, &target->views, link)
        wlr_scene_node_set_enabled(&view->scene_tree->node, true);
    target->is_active = true;
    output->active_workspace = target;
    
    wlr_seat_keyboard_clear_focus(output->server->seat);
    output->server->focused_view = NULL;
    
    if (!wl_list_empty(&target->views)) {
        struct hermit_view *orig = wl_container_of(target->views.next, orig, link);
        struct wlr_keyboard *kb = wlr_seat_get_keyboard(output->server->seat);
        wlr_seat_keyboard_notify_enter(output->server->seat, orig->xdg_toplevel->base->surface,
                                       kb ? kb->keycodes : NULL, kb ? kb->num_keycodes : 0,
                                       kb ? &kb->modifiers : NULL);
        output->server->focused_view = orig;
    }
    
}

void hermit_workspace_move_view(struct hermit_view *view, int index) {
    if (!view->workspace) return;
    
    struct hermit_output *output = view->workspace->output;
    struct hermit_workspace *target = NULL;
    
    for (int i=0; i<output->workspace_count; i++) {
        if (output->workspaces[i].index == index) {
            target = &output->workspaces[i];
            break;
        }
    }
    
    if (!target || target == view->workspace) return;
    
    wl_list_remove(&view->link);
    wlr_scene_node_set_enabled(&view->scene_tree->node, target->is_active);
    wl_list_insert(&target->views, &view->link);
    view->workspace = target;
}