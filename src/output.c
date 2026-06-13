#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <time.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output_layout.h>
#include <hermit/output.h>
#include <hermit/server.h>
#include <hermit/config.h>
#include <hermit/logger.h>
#include <limits.h>

static void output_frame(struct wl_listener *listener, void *data) {
    struct hermit_output *output = wl_container_of(listener, output, frame);
    struct wlr_scene *scene = output->server->scene;
    struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, output->wlr_output);
    
    wlr_scene_output_commit(scene_output, NULL);
    
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
    struct hermit_output *output = wl_container_of(listener, output, request_state);
    const struct wlr_output_event_request_state *event = data;
    wlr_output_commit_state(output->wlr_output, event->state);
}

static void output_destroy(struct wl_listener *listener, void *data) {
    struct hermit_output *output = wl_container_of(listener, output, destroy);
    wl_list_remove(&output->frame.link);
    wl_list_remove(&output->request_state.link);
    wl_list_remove(&output->destroy.link);
    wl_list_remove(&output->link);
    free(output);
}

static struct hermit_monitor_config *find_monitor_config(struct hermit_server *server, const char *name) {
    struct hermit_monitor_config *wildcard = NULL;
    for (int i=0; i<server->config->monitor_count; i++) {
        struct hermit_monitor_config *mon = &server->config->monitors[i];
        if (strcmp(mon->name, name) == 0) return mon;
        if (strcmp(mon->name, "*") == 0) wildcard = mon;
    }
    return wildcard;
}

void hermit_outputs_apply_config(struct hermit_server *server) {
    struct hermit_output *output;
    wl_list_for_each(output, &server->outputs, link) {
        struct hermit_monitor_config *mon =
            find_monitor_config(server, output->wlr_output->name);
        if (!mon) continue;

        struct wlr_output_state state;
        wlr_output_state_init(&state);
        wlr_output_state_set_enabled(&state, true);

        if (mon->width > 0 && mon->height > 0) {
            struct wlr_output_mode *mode, *best = NULL;
            wl_list_for_each(mode, &output->wlr_output->modes, link) {
                if (mode->width == mon->width && mode->height == mon->height) {
                    int diff_mode = abs(mode->refresh - mon->refresh * 1000);
                    int diff_best = best ?
                        abs(best->refresh - mon->refresh * 1000) : INT_MAX;
                    if (!best || (mon->refresh > 0 && diff_mode < diff_best))
                        best = mode;
                }
            }
            if (best) {
                hlog_info("Applying mode %dx%d@%dmHz on %s",
                    best->width, best->height, best->refresh,
                    output->wlr_output->name);
                wlr_output_state_set_mode(&state, best);
            }
        }

        bool ok = wlr_output_commit_state(output->wlr_output, &state);
        hlog_error("Mode commit for %s: %s",
            output->wlr_output->name, ok ? "SUCCEEDED" : "FAILED");
        wlr_output_state_finish(&state);

        if (mon->x != 0 || mon->y != 0) {
            wlr_output_layout_add(server->output_layout,
                output->wlr_output, mon->x, mon->y);
        }
    }
}

static void server_new_output(struct wl_listener *listener, void *data) {
    struct hermit_server *server =
        wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    wlr_output_init_render(wlr_output, server->allocator, server->renderer);

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode)
        wlr_output_state_set_mode(&state, mode);
    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    struct hermit_output *output = calloc(1, sizeof(*output));
    output->server = server;
    output->wlr_output = wlr_output;

    output->frame.notify = output_frame;
    wl_signal_add(&wlr_output->events.frame, &output->frame);
    output->request_state.notify = output_request_state;
    wl_signal_add(&wlr_output->events.request_state, &output->request_state);
    output->destroy.notify = output_destroy;
    wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    wl_list_insert(&server->outputs, &output->link);

    struct wlr_output_layout_output *layout_output =
        wlr_output_layout_add_auto(server->output_layout, wlr_output);
    struct wlr_scene_output *scene_output =
        wlr_scene_output_create(server->scene, wlr_output);
    wlr_scene_output_layout_add_output(server->scene_output_layout,
        layout_output, scene_output);
}

void hermit_output_manager_init(struct hermit_server *server) {
    wl_list_init(&server->outputs);
    server->new_output.notify = server_new_output;
    wl_signal_add(&server->backend->events.new_output, &server->new_output);
}