#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <hermit/view.h>
#include <hermit/server.h>

static void view_map(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, map);
    wlr_log(WLR_DEBUG, "View mapped: %s",
        view->xdg_toplevel->title ? view->xdg_toplevel->title : "untitled");

    struct wlr_box geo;
    wlr_surface_get_extents(view->xdg_toplevel->base->surface, &geo);
    wlr_log(WLR_DEBUG, "View geometry: %dx%d at %d,%d",
        geo.width, geo.height, geo.x, geo.y);

    wl_list_insert(&view->server->views, &view->link);

    // give it keyboard focus immediately on map
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(view->server->seat);
    wlr_seat_keyboard_notify_enter(view->server->seat,
        view->xdg_toplevel->base->surface,
        keyboard ? keyboard->keycodes : NULL,
        keyboard ? keyboard->num_keycodes : 0,
        keyboard ? &keyboard->modifiers : NULL);
}

static void view_unmap(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, unmap);
    wl_list_remove(&view->link);
}

static void view_destroy(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, destroy);
    wl_list_remove(&view->map.link);
    wl_list_remove(&view->unmap.link);
    wl_list_remove(&view->destroy.link);
    wl_list_remove(&view->request_maximize.link);
    wl_list_remove(&view->request_fullscreen.link);
    wl_list_remove(&view->commit.link);
    if (view->link.next) {
        wl_list_remove(&view->link);
    }
    free(view);
}

static void view_commit(struct wl_listener *listener, void *data) {
    struct hermit_view *view = wl_container_of(listener, view, commit);
    
    if (!view->initial_configure_sent) {
        wlr_xdg_surface_schedule_configure(view->xdg_toplevel->base);
        view->initial_configure_sent = true;
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