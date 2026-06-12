#include <stdlib.h>
#include <stdbool.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/log.h>
#include <hermit/server.h>
#include <hermit/output.h>
#include <hermit/view.h>
#include <hermit/input.h>

bool hermit_server_init(struct hermit_server *server) {
    server->display = wl_display_create();
    if (!server->display) {
        wlr_log(WLR_ERROR, "Failed to create Wayland display.");
        return false;
    }
    server->backend = wlr_backend_autocreate(wl_display_get_event_loop(server->display), NULL);
    if (!server->backend) {
        wlr_log(WLR_ERROR, "Failed to create wlroots backend.");
        return false;
    }
    server->renderer = wlr_renderer_autocreate(server->backend);
    if (!server->renderer) {
        wlr_log(WLR_ERROR, "Failed to create renderer.");
        return false;
    }
    wlr_renderer_init_wl_display(server->renderer, server->display);
    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
    if (!server->allocator) {
        wlr_log(WLR_ERROR, "Failed to create allocator.");
        return false;
    }
    server->compositor = wlr_compositor_create(server->display, 6, server->renderer);
    wlr_subcompositor_create(server->display);
    
    server->output_layout = wlr_output_layout_create(server->display);
    server->scene = wlr_scene_create();
    server->scene_output_layout = wlr_scene_attach_output_layout(server->scene, server->output_layout);
    hermit_output_manager_init(server);
    hermit_view_manager_init(server);
    hermit_input_manager_init(server);
    return true;
}

void hermit_server_run(struct hermit_server *server) {
    const char *socket = wl_display_add_socket_auto(server->display);
    if (!socket) {
        wlr_log(WLR_ERROR, "Failed to create Wayland socket.");
        return;
    }
    snprintf(server->socket, sizeof(server->socket), "%s", socket);
    if (!wlr_backend_start(server->backend)) {
        wlr_log(WLR_ERROR, "Failed to start wlroots backend.");
        return;
    }
    
    hermit_outputs_apply_config(server);

    wlr_log(WLR_INFO, "hermit-comp running on WAYLAND_DISPLAY=%s", socket);
    wl_display_run(server->display);
}

void hermit_server_end(struct hermit_server *server) {
    wl_display_destroy_clients(server->display);
    wlr_scene_node_destroy(&server->scene->tree.node);
    wlr_output_layout_destroy(server->output_layout);
    wlr_allocator_destroy(server->allocator);
    wlr_renderer_destroy(server->renderer);
    wlr_backend_destroy(server->backend);
    wl_display_destroy(server->display);
}