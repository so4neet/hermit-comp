#pragma once

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <hermit/config.h>

struct hermit_server {
    struct wl_display               *display;
    struct wlr_session               *session;
    struct wlr_backend              *backend;
    struct wlr_renderer             *renderer;
    struct wlr_allocator            *allocator;
    struct wlr_compositor           *compositor;
    struct wlr_scene                *scene;
    struct wlr_output_layout        *output_layout;
    struct wlr_scene_output_layout  *scene_output_layout;
    struct wlr_xdg_shell            *xdg_shell;
    struct wlr_seat                 *seat;
    struct wlr_cursor               *cursor;
    struct wlr_xcursor_manager      *cursor_mgr;
    struct wl_list                   outputs;
    struct wl_list                   views;
    struct wl_list                   keyboards;
    struct wl_listener               new_output;
    struct wl_listener               new_xdg_toplevel;
    struct wl_listener               new_input;
    struct wl_listener               cursor_motion;
    struct wl_listener               cursor_motion_absolute;
    struct wl_listener               cursor_button;
    struct wl_listener               cursor_axis;
    struct wl_listener               cursor_frame;
    struct hermit_config             *config;
    struct hermit_view               *focused_view;
    char   socket[256];
};

bool hermit_server_init(struct hermit_server *server);
void hermit_server_run (struct hermit_server *server);
void hermit_server_end (struct hermit_server *server);