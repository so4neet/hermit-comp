#pragma once

#include <stdint.h>
#include <stdbool.h>

#define HERMIT_MAX_BINDS        256
#define HERMIT_MAX_FLOAT_RULES  64
#define HERMIT_MAX_AUTOSTART    32
#define HERMIT_MAX_MONITORS     16

enum hermit_mode {
    HERMIT_MODE_FLOATING,
    HERMIT_MODE_TILING,
};

enum hermit_bind_action {
    HERMIT_ACTION_EXEC,
    HERMIT_ACTION_CLOSE,
    HERMIT_ACTION_QUIT,
    HERMIT_ACTION_TOGGLE_MODE,
    HERMIT_ACTION_MOVE_WINDOW,
    HERMIT_ACTION_FOCUS,
    HERMIT_ACTION_WORKSPACE,
    HERMIT_ACTION_MOVE_TO_WORKSPACE,
};

enum hermit_direction {
    HERMIT_DIR_LEFT,
    HERMIT_DIR_RIGHT,
    HERMIT_DIR_UP,
    HERMIT_DIR_DOWN,
};

struct hermit_keybind {
    uint32_t modifiers;
    uint32_t key;
    enum hermit_bind_action action;
    char arg[256];
};

struct hermit_float_rule {
    char app_id[256];
};

struct hermit_monitor_config {
    char name[64];
    int width;
    int height;
    int refresh;
    int x;
    int y;
    bool disabled;
    int ws_start;
    int ws_end;
};

struct hermit_config {
    enum hermit_mode default_mode;
    struct hermit_keybind keybinds[HERMIT_MAX_BINDS];
    int keybind_count;
    struct hermit_float_rule float_rules[HERMIT_MAX_FLOAT_RULES];
    int float_rule_count;
    char autostart[HERMIT_MAX_AUTOSTART][256];
    int autostart_count;
    struct hermit_monitor_config monitors[HERMIT_MAX_MONITORS];
    int monitor_count;
    float split_ratio;
    int force_float_max_width;
    int force_float_max_height;
    int gaps_inner;
    int gaps_outer;
    int border_width;
    uint32_t border_color_active;
    uint32_t border_color_inactive;
};

struct hermit_config *hermit_config_load(const char *path);
void hermit_config_destroy(struct hermit_config *config);