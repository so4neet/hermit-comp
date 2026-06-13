#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <hermit/view.h>
#include <hermit/server.h>
#include <hermit/output.h>

static void view_map(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, map);
    
    struct wlr_output *wlr_output = wlr_output_layout_output_at(
        view->server->output_layout,
        view->server->cursor->x,
        view->server->cursor->y);
        
    struct hermit_output *output = NULL;
    struct hermit_output *o;
    wl_list_for_each(o, &view->server->outputs, link) {
        if (o->wlr_output == wlr_output) {
            output = o;
            break;
        }
    }
    
    if (output && output->active_workspace) {
        view->workspace = output->active_workspace;
        wl_list_insert(&output->active_workspace->views, &view->link);
        bsp_insert(output->active_workspace, view);
    } else {
        wl_list_insert(&view->server->views, &view->link);
    }
    
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(view->server->seat);
    wlr_seat_keyboard_notify_enter(view->server->seat,
        view->xdg_toplevel->base->surface,
        keyboard ? keyboard->keycodes : NULL,
        keyboard ? keyboard->num_keycodes : 0,
        keyboard ? &keyboard->modifiers : NULL);
    view->server->focused_view = view;
}

static void view_unmap(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, unmap);
    struct hermit_server *server = view->server;
    
    bool was_focused = (server->focused_view == view);
    struct hermit_view *next_focus = NULL;

    if (view->workspace) {
        struct hermit_bsp_node *leaf = bsp_find_leaf(view->workspace->bsp_root, view);
        if (leaf && leaf->parent) {
            struct hermit_bsp_node *parent = leaf->parent;
            struct hermit_bsp_node *sibling = (parent->split.left == leaf) ? parent->split.right : parent->split.left;
            
            struct hermit_bsp_node *curr = sibling;
            while (curr && curr->type == BSP_SPLIT) {
                curr = curr->split.left; 
            }
            if (curr && curr->type == BSP_LEAF) {
                next_focus = curr->leaf.view;
            }
        }
        bsp_remove(view->workspace, view);
    }
    
    wl_list_remove(&view->link);
    view->workspace = NULL;
    if (was_focused) {
        server->focused_view = NULL;
        
        if (next_focus && next_focus->xdg_toplevel && next_focus->xdg_toplevel->base->surface) {
            struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
            wlr_seat_keyboard_notify_enter(server->seat,
                next_focus->xdg_toplevel->base->surface,
                keyboard ? keyboard->keycodes : NULL,
                keyboard ? keyboard->num_keycodes : 0,
                keyboard ? &keyboard->modifiers : NULL);
            server->focused_view = next_focus;
        } else {
            wlr_seat_keyboard_clear_focus(server->seat);
        }
    }
}

static void view_destroy(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, destroy);
    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->request_maximize.link);
    wl_list_remove(&view->request_fullscreen.link);
    wl_list_remove(&view->commit.link);
    if (view->server->focused_view == view)
        view->server->focused_view = NULL;
    free(view);
}

static void view_commit(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, commit);
    
    if (!view->initial_configure_sent) {
        view->initial_configure_sent = true;
        wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
        return;
    }

    if (view->workspace && view->workspace->bsp_root) {
        struct hermit_server *server = view->server;
        struct wlr_box output_box;
        wlr_output_layout_get_box(server->output_layout, view->workspace->output->wlr_output, &output_box);
        
        bsp_apply_layout(view->workspace->bsp_root, output_box, 
                         server->config->gaps_inner, server->config->gaps_outer, true);
    }
}

static void view_request_maximize(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, request_maximize);
    wlr_xdg_toplevel_set_maximized(view->xdg_toplevel, view->xdg_toplevel->requested.maximized);
}

static void view_request_fullscreen(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, request_fullscreen);
    wlr_xdg_toplevel_set_fullscreen(view->xdg_toplevel, view->xdg_toplevel->requested.fullscreen);
}

static void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
    struct hermit_server *server = wl_container_of(listener, server, new_xdg_toplevel);
    struct wlr_xdg_toplevel *xdg_toplevel = data;
    
    struct hermit_view *view = calloc(1, sizeof(*view));
    view->server = server;
    view->xdg_toplevel = xdg_toplevel;
    view->scene_tree = wlr_scene_xdg_surface_create(&server->scene->tree, xdg_toplevel->base);
    view->scene_tree->node.data = view;
    xdg_toplevel->base->data = view->scene_tree;
    view->commit.notify = view_commit;
    wl_signal_add(&xdg_toplevel->base->surface->events.commit, &view->commit);
    
    view->map.notify = view_map;
    wl_signal_add(&xdg_toplevel->base->surface->events.map, &view->map);
    
    view->unmap.notify = view_unmap;
    wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &view->unmap);
    
    view->destroy.notify = view_destroy;
    wl_signal_add(&xdg_toplevel->events.destroy, &view->destroy);
    
    view->request_maximize.notify = view_request_maximize;
    wl_signal_add(&xdg_toplevel->events.request_maximize, &view->request_maximize);
    
    view->request_fullscreen.notify = view_request_fullscreen;
    wl_signal_add(&xdg_toplevel->events.request_fullscreen, &view->request_fullscreen);
}

void hermit_view_manager_init(struct hermit_server *server) {
    wl_list_init(&server->views);
    server->xdg_shell = wlr_xdg_shell_create(server->display, 3);
    server->new_xdg_toplevel.notify = server_new_xdg_toplevel;
    wl_signal_add(&server->xdg_shell->events.new_toplevel, &server->new_xdg_toplevel);
}