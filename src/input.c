#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_input_device.h>
#include <xkbcommon/xkbcommon.h>
#include <hermit/input.h>
#include <hermit/server.h>
#include <hermit/view.h>
#include <hermit/config.h>

static struct hermit_view *view_at(struct hermit_server *server,
        double lx, double ly,
        struct wlr_surface **surface,
        double *sx, double *sy) {
    struct wlr_scene_node *node = wlr_scene_node_at(
        &server->scene->tree.node, lx, ly, sx, sy);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER)
        return NULL;
    struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
    struct wlr_scene_surface *scene_surface =
        wlr_scene_surface_try_from_buffer(scene_buffer);
    if (!scene_surface)
        return NULL;
    *surface = scene_surface->surface;
    struct wlr_scene_tree *tree = node->parent;
    while (tree && !tree->node.data)
        tree = tree->node.parent;
    return tree ? tree->node.data : NULL;
}

static void process_cursor_motion(struct hermit_server *server, uint32_t time) {
    double sx, sy;
    struct wlr_surface *surface = NULL;
    struct hermit_view *view = view_at(server,
        server->cursor->x, server->cursor->y, &surface, &sx, &sy);

    if (!view) {
        wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
    }

    if (surface) {
        wlr_seat_pointer_notify_enter(server->seat, surface, sx, sy);
        wlr_seat_pointer_notify_motion(server->seat, time, sx, sy);
    } else {
        wlr_seat_pointer_clear_focus(server->seat);
    }
}

static void keyboard_modifiers(struct wl_listener *listener, void *data) {
    struct hermit_keyboard *keyboard = wl_container_of(listener, keyboard, modifiers);
    wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_modifiers(keyboard->server->seat, &keyboard->wlr_keyboard->modifiers);
}

static bool handle_keybind(struct hermit_server *server, uint32_t mods, xkb_keysym_t sym) {
    sym = xkb_keysym_to_lower(sym);
    
    mods &= ~WLR_MODIFIER_CAPS;
    mods &= ~WLR_MODIFIER_MOD2;
    
    struct hermit_config *config = server->config;
        wlr_log(WLR_INFO, "handle_keybind: mods=0x%x sym=0x%x keybind_count=%d",
        mods, sym, server->config->keybind_count);
    for (int i=0; i<config->keybind_count; i++) {
        struct hermit_keybind *bind = &config->keybinds[i];
                wlr_log(WLR_INFO, "  bind[%d]: mods=0x%x sym=0x%x",
            i, bind->modifiers, bind->key);
        if (bind->modifiers == mods && bind->key == sym) {
                        wlr_log(WLR_INFO, "  MATCHED bind[%d]", i);
            switch (bind->action) {
                case HERMIT_ACTION_EXEC:
                    wlr_log(WLR_INFO, "Executing %s", bind->arg);
                    if (fork() == 0) {
                        setenv("WAYLAND_DISPLAY", server->socket, true);
                        execl("/bin/sh", "/bin/sh", "-c", bind->arg, NULL);
                    }
                    break;
                case HERMIT_ACTION_QUIT:
                    wl_display_terminate(server->display);
                    break;
                case HERMIT_ACTION_CLOSE:
                    if (!wl_list_empty(&server->views)) {
                        struct hermit_view *view;
                        wl_list_for_each(view, &server->views, link) {
                            wlr_xdg_toplevel_send_close(view->xdg_toplevel);
                            break;
                        }
                    }
                    break;
            }
            return true;
          }
    }
    return false;
}

static void keyboard_key(struct wl_listener *listener, void *data) {
    struct hermit_keyboard *keyboard = wl_container_of(listener, keyboard, key);
    struct wlr_keyboard_key_event *event = data;
    struct wlr_seat *seat = keyboard->server->seat;
    
    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);
    bool handled = false;
    uint32_t mods = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int i=0; i<nsyms; i++) {
            handled = handle_keybind(keyboard->server, mods, syms[i]);
            if (handled) break;
        }
    }
    if (!handled) {
        wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
        wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
    }
}

static void keyboard_destroy(struct wl_listener *listener, void *data) {
    struct hermit_keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
    wl_list_remove(&keyboard->modifiers.link);
    wl_list_remove(&keyboard->key.link);
    wl_list_remove(&keyboard->destroy.link);
    wl_list_remove(&keyboard->link);
    free(keyboard);
}

static void server_new_keyboard(struct hermit_server *server,
        struct wlr_input_device *device) {
    struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

    struct hermit_keyboard *keyboard = calloc(1, sizeof(*keyboard));
    keyboard->server = server;
    keyboard->wlr_keyboard = wlr_keyboard;

    struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap *keymap = xkb_keymap_new_from_names(
        context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
    wlr_keyboard_set_keymap(wlr_keyboard, keymap);
    xkb_keymap_unref(keymap);
    xkb_context_unref(context);
    wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

    keyboard->modifiers.notify = keyboard_modifiers;
    wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);

    keyboard->key.notify = keyboard_key;
    wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

    keyboard->destroy.notify = keyboard_destroy;
    wl_signal_add(&device->events.destroy, &keyboard->destroy);

    wlr_seat_set_keyboard(server->seat, wlr_keyboard);
    wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_pointer(struct hermit_server *server,
        struct wlr_input_device *device) {
    wlr_cursor_attach_input_device(server->cursor, device);
}

static void server_new_input(struct wl_listener *listener, void *data) {
    struct hermit_server *server =
        wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;

    switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
        server_new_keyboard(server, device);
        break;
    case WLR_INPUT_DEVICE_POINTER:
        server_new_pointer(server, device);
        break;
    default:
        break;
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!wl_list_empty(&server->keyboards))
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    wlr_seat_set_capabilities(server->seat, caps);
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
    struct hermit_server *server =
        wl_container_of(listener, server, cursor_motion);
    struct wlr_pointer_motion_event *event = data;
    wlr_cursor_move(server->cursor, &event->pointer->base,
        event->delta_x, event->delta_y);
    process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(struct wl_listener *listener, void *data) {
    struct hermit_server *server =
        wl_container_of(listener, server, cursor_motion_absolute);
    struct wlr_pointer_motion_absolute_event *event = data;
    wlr_cursor_warp_absolute(server->cursor, &event->pointer->base,
        event->x, event->y);
    process_cursor_motion(server, event->time_msec);
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
    struct hermit_server *server =
        wl_container_of(listener, server, cursor_button);
    struct wlr_pointer_button_event *event = data;

    wlr_seat_pointer_notify_button(server->seat,
        event->time_msec, event->button, event->state);

    double sx, sy;
    struct wlr_surface *surface = NULL;
    struct hermit_view *view = view_at(server,
        server->cursor->x, server->cursor->y, &surface, &sx, &sy);

    if (event->state == WL_POINTER_BUTTON_STATE_PRESSED && view) {
        // focus the keyboard on this view
        struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
        wlr_seat_keyboard_notify_enter(server->seat,
            view->xdg_toplevel->base->surface,
            keyboard ? keyboard->keycodes : NULL,
            keyboard ? keyboard->num_keycodes : 0,
            keyboard ? &keyboard->modifiers : NULL);
    }
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
    struct hermit_server *server =
        wl_container_of(listener, server, cursor_axis);
    struct wlr_pointer_axis_event *event = data;
    wlr_seat_pointer_notify_axis(server->seat, event->time_msec,
        event->orientation, event->delta, event->delta_discrete, event->source,
        event->relative_direction);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
    struct hermit_server *server =
        wl_container_of(listener, server, cursor_frame);
    wlr_seat_pointer_notify_frame(server->seat);
}

void hermit_input_manager_init(struct hermit_server *server) {
    wl_list_init(&server->keyboards);

    server->cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server->cursor, server->output_layout);

    server->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    server->cursor_motion.notify = server_cursor_motion;
    wl_signal_add(&server->cursor->events.motion, &server->cursor_motion);

    server->cursor_motion_absolute.notify = server_cursor_motion_absolute;
    wl_signal_add(&server->cursor->events.motion_absolute,
        &server->cursor_motion_absolute);

    server->cursor_button.notify = server_cursor_button;
    wl_signal_add(&server->cursor->events.button, &server->cursor_button);

    server->cursor_axis.notify = server_cursor_axis;
    wl_signal_add(&server->cursor->events.axis, &server->cursor_axis);

    server->cursor_frame.notify = server_cursor_frame;
    wl_signal_add(&server->cursor->events.frame, &server->cursor_frame);

    server->new_input.notify = server_new_input;
    wl_signal_add(&server->backend->events.new_input, &server->new_input);

    server->seat = wlr_seat_create(server->display, "seat0");
}
