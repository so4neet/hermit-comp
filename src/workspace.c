#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_scene.h>
#include <hermit/workspace.h>
#include <hermit/server.h>
#include <hermit/output.h>
#include <hermit/view.h>
#include <hermit/config.h>

struct hermit_workspace *hermit_workspace_get(
        struct hermit_server *server, int index) {
    struct hermit_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        for (int i = 0; i < output->workspace_count; i++) {
            if (output->workspaces[i].index == index)
                return &output->workspaces[i];
        }
    }
    return NULL;
}

struct hermit_output *hermit_output_for_workspace(
        struct hermit_server *server, int index) {
    struct hermit_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        for (int i = 0; i < output->workspace_count; i++) {
            if (output->workspaces[i].index == index)
                return output;
        }
    }
    return NULL;
}

void hermit_workspaces_init(struct hermit_server *server) {
    struct hermit_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        struct hermit_monitor_config *mon = NULL;
        for (int i = 0; i < server->config->monitor_count; i++) {
            if (strcmp(server->config->monitors[i].name,
                    output->wlr_output->name) == 0) {
                mon = &server->config->monitors[i];
                break;
            }
        }

        int ws_start = mon ? mon->ws_start : 1;
        int ws_end   = mon ? mon->ws_end   : 5;
        int count    = ws_end - ws_start + 1;

        output->workspace_count = count;

        for (int i = 0; i < count; i++) {
            struct hermit_workspace *ws = &output->workspaces[i];
            ws->index     = ws_start + i;
            ws->output    = output;
            ws->is_active = (i == 0);
            wl_list_init(&ws->views);
            wlr_log(WLR_INFO, "Initialized workspace %d on output %s",
                ws->index, output->wlr_output->name);
        }

        output->active_workspace = &output->workspaces[0];
    }
}

void hermit_workspace_switch(struct hermit_output *output, int index) {
    struct hermit_workspace *target = NULL;
    for (int i = 0; i < output->workspace_count; i++) {
        if (output->workspaces[i].index == index) {
            target = &output->workspaces[i];
            break;
        }
    }

    if (!target) {
        wlr_log(WLR_ERROR, "Workspace %d not found on output %s",
            index, output->wlr_output->name);
        return;
    }

    if (target == output->active_workspace) return;

    wlr_log(WLR_INFO, "Switching from workspace %d to %d on %s",
        output->active_workspace->index, index,
        output->wlr_output->name);
    struct hermit_view *view;
    wl_list_for_each(view, &output->active_workspace->views, link) {
        wlr_scene_node_set_enabled(&view->scene_tree->node, false);
    }
    output->active_workspace->is_active = false;

    wl_list_for_each(view, &target->views, link) {
        wlr_scene_node_set_enabled(&view->scene_tree->node, true);
    }
    target->is_active = true;
    output->active_workspace = target;

    wlr_seat_keyboard_clear_focus(output->server->seat);
    output->server->focused_view = NULL;
    
    struct hermit_view *tview;
    wl_list_for_each(view, &output->active_workspace->views, link) {
        wlr_scene_node_set_enabled(&view->scene_tree->node, false);
    }
    output->active_workspace->is_active = false;
    
    wl_list_for_each(tview, &target->views, link) {
        wlr_scene_node_set_enabled(&tview->scene_tree->node, true);
    }
    target->is_active = true;
    output->active_workspace = target;

    if (!wl_list_empty(&target->views)) {
        struct hermit_view *first = wl_container_of(
            target->views.next, first, link);
        struct wlr_keyboard *keyboard =
            wlr_seat_get_keyboard(output->server->seat);
        wlr_seat_keyboard_notify_enter(output->server->seat,
            first->xdg_toplevel->base->surface,
            keyboard ? keyboard->keycodes : NULL,
            keyboard ? keyboard->num_keycodes : 0,
            keyboard ? &keyboard->modifiers : NULL);
        output->server->focused_view = first;
    }
    /* TODO: vertical slide animation */
}

void hermit_workspace_move_view(struct hermit_view *view, int index) {
    struct hermit_server *server = view->server;
    struct hermit_workspace *target =
        hermit_workspace_get(server, index);

    if (!target) {
        wlr_log(WLR_ERROR, "Workspace %d not found", index);
        return;
    }

    if (target == view->workspace) return;

    wl_list_remove(&view->link);

    if (!target->is_active) {
        wlr_scene_node_set_enabled(&view->scene_tree->node, false);
    } else {
        wlr_scene_node_set_enabled(&view->scene_tree->node, true);
    }

    wl_list_insert(&target->views, &view->link);
    view->workspace = target;

    wlr_log(WLR_INFO, "Moved view to workspace %d", index);
}