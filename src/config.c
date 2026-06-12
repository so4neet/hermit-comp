#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input-event-codes.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_keyboard.h>
#include <hermit/config.h>

static uint32_t parse_modifier(const char *name) {
    if (strcasecmp(name, "SUPER") == 0 || strcasecmp(name, "MOD4") == 0)
        return WLR_MODIFIER_LOGO;
    if (strcasecmp(name, "ALT") == 0 || strcasecmp(name, "MOD1") == 0)
        return WLR_MODIFIER_ALT;
    if (strcasecmp(name, "CTRL") == 0 || strcasecmp(name, "CONTROL") == 0)
        return WLR_MODIFIER_CTRL;
    if (strcasecmp(name, "SHIFT") == 0)
        return WLR_MODIFIER_SHIFT;
    wlr_log(WLR_ERROR, "Unknown mod: %s", name);
    return 0;
}

static bool parse_key_combo(const char *combo, uint32_t *mods_out, uint32_t *key_out) {
    char buf[256];
    strncpy(buf, combo, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    *mods_out = 0;
    *key_out = 0;
    char *token = strtok(buf, "+");
    char *last = NULL;
    while (token) {
        char *next = strtok(NULL, "+");
        if (next) {
            *mods_out |= parse_modifier(token);
        } else {
            last = token;
        }
        token = next;
    }
    
    if (!last) {
        wlr_log(WLR_ERROR, "No key in combo: %s", combo);
        return false;
    }
    
    xkb_keysym_t sym = xkb_keysym_from_name(last, XKB_KEYSYM_CASE_INSENSITIVE);
    if (sym == XKB_KEY_NoSymbol) {
        wlr_log(WLR_ERROR, "Unknown key: %s", last);
        return false;
    }
    *key_out = sym;
    return true;
}

static bool parse_action(const char *action_str, const char *arg, struct hermit_keybind *bind) {
    if (strcasecmp(action_str, "exec") == 0) {
        bind->action = HERMIT_ACTION_EXEC;
        strncpy(bind->arg, arg ? arg : "", sizeof(bind->arg)-1);
    } else if (strcasecmp(action_str, "close") == 0) {
        bind->action = HERMIT_ACTION_CLOSE;
    } else if (strcasecmp(action_str, "quit") == 0) {
        bind->action = HERMIT_ACTION_QUIT;
    } else if (strcasecmp(action_str, "toggle_mode") == 0) {
        bind->action = HERMIT_ACTION_TOGGLE_MODE;
    } else if (strcasecmp(action_str, "move_window") == 0) {
        bind->action = HERMIT_ACTION_MOVE_WINDOW;
        strncpy(bind->arg, arg ? arg : "", sizeof(bind->arg)-1);
    } else if (strcasecmp(action_str, "focus") == 0) {
        bind->action = HERMIT_ACTION_FOCUS;
        strncpy(bind->arg, arg ? arg : "", sizeof(bind->arg)-1);
    } else if (strcasecmp(action_str, "workspace") == 0) {
        bind->action = HERMIT_ACTION_WORKSPACE;
        strncpy(bind->arg, arg ? arg : "", sizeof(bind->arg)-1);
    } else if (strcasecmp(action_str, "move_to_workspace") == 0) {
        bind->action = HERMIT_ACTION_MOVE_TO_WORKSPACE;
        strncpy(bind->arg, arg ? arg : "", sizeof(bind->arg)-1);
    } else {
        wlr_log(WLR_ERROR, "Unknown action: %s", action_str);
        return false;
    }
    return true;
}

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

static uint32_t parse_color(const char *s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return (uint32_t)strtoul(s + 2, NULL, 16);
    return (uint32_t)strtoul(s, NULL, 16);
}

static void parse_line(struct hermit_config *config, const char *key, const char *value) {
    if (strcmp(key, "default_mode") == 0) {
        if (strcasecmp(value, "tiling") == 0)
            config->default_mode = HERMIT_MODE_TILING;
        else
            config->default_mode = HERMIT_MODE_FLOATING;
    } else if (strcmp(key, "bind") == 0) {
        if (config->keybind_count >= HERMIT_MAX_BINDS) {
            wlr_log(WLR_ERROR, "Max keybinds met.");
            return;
        }
        
        char buf[512];
        strncpy(buf, value, sizeof(buf)-1);
        
        char *combo = strtok(buf, ",");
        char *action = strtok(NULL, ",");
        char *arg = strtok(NULL, ",");
        
        if (!combo || !action) {
            wlr_log(WLR_ERROR, "Invalid bind: %s", value);
            return;
        }
        
        combo = trim(combo);
        action = trim(action);
        if (arg) arg = trim(arg);
        
        struct hermit_keybind *bind = &config->keybinds[config->keybind_count];
        if (!parse_key_combo(combo, &bind->modifiers, &bind->key))
            return;
        if (!parse_action(action, arg, bind))
            return;
        config->keybind_count++;
    }else if (strcmp(key, "float_rule") == 0) {
        if (config->float_rule_count >= HERMIT_MAX_FLOAT_RULES)
            return;
        strncpy(config->float_rules[config->float_rule_count].app_id,
            value, 255);
        config->float_rule_count++;

    } else if (strcmp(key, "autostart") == 0) {
        if (config->autostart_count >= HERMIT_MAX_AUTOSTART)
            return;
        strncpy(config->autostart[config->autostart_count], value, 255);
        config->autostart_count++;

    } else if (strcmp(key, "split_ratio") == 0) {
        config->split_ratio = atof(value);

    } else if (strcmp(key, "force_float_max_size") == 0) {
        sscanf(value, "%dx%d",
            &config->force_float_max_width, &config->force_float_max_height);

    } else if (strcmp(key, "gaps_inner") == 0) {
        config->gaps_inner = atoi(value);

    } else if (strcmp(key, "gaps_outer") == 0) {
        config->gaps_outer = atoi(value);

    } else if (strcmp(key, "border_width") == 0) {
        config->border_width = atoi(value);

    } else if (strcmp(key, "border_color_active") == 0) {
        config->border_color_active = parse_color(value);

    } else if (strcmp(key, "border_color_inactive") == 0) {
        config->border_color_inactive = parse_color(value);

    } else if (strcmp(key, "monitor") == 0) {
        if (config->monitor_count >= HERMIT_MAX_MONITORS) {
            wlr_log(WLR_ERROR, "Too many monitors. If you somehow reached this limit submit and issue, weirdo :P");
            return;
        }
        struct hermit_monitor_config *mon = &config->monitors[config->monitor_count];
        
        mon->ws_start = 1;
        mon->ws_end   = 5;
        
        char buf[512];
        strncpy(buf, value, sizeof(buf)-1);
        char *name_s = strtok(buf, ",");
        char *res_s  = strtok(NULL, ",");
        char *ref_s  = strtok(NULL, ",");
        char *x_s    = strtok(NULL, ",");
        char *y_s    = strtok(NULL, ",");
        char *ws_s   = strtok(NULL, ",");
        
        if (!name_s) {
            wlr_log(WLR_ERROR, "Invalid monitor config: %s", value);
            return;
        }
        
        strncpy(mon->name, trim(name_s), sizeof(mon->name)-1);
        
        if (res_s) {
            res_s = trim(res_s);
            if (strcasecmp(res_s, "preferred") == 0) {
                mon->width = 0;
                mon->height = 0;
            } else {
                sscanf(res_s, "%dx%d", &mon->width, &mon->height);
            }
        }
        
        if (ref_s) {
            ref_s = trim(ref_s);
            if (strcasecmp(ref_s, "preferred") == 0) {
                mon->refresh = 0;
            } else {
                mon->refresh = atoi(ref_s);
            }
        }
        
        if (x_s) mon->x = atoi(trim(x_s));
        if (y_s) mon->y = atoi(trim(y_s));
        
        if (ws_s) {
            ws_s = trim(ws_s);
            char *eq = strchr(ws_s, '=');
            if (eq) ws_s = eq + 1;
            sscanf(ws_s, "%d-%d", &mon->ws_start, &mon->ws_end);
        }
        
        config->monitor_count++;
    } else {
        wlr_log(WLR_ERROR, "Unknown config key: %s", key);
    }
}

static void config_set_defaults(struct hermit_config *config) {
    config->default_mode          = HERMIT_MODE_FLOATING;
    config->split_ratio           = 0.5f;
    config->force_float_max_width = 400;
    config->force_float_max_height= 400;
    config->gaps_inner            = 8;
    config->gaps_outer            = 16;
    config->border_width          = 2;
    config->border_color_active   = 0x6699ccff;
    config->border_color_inactive = 0x333333ff;
}

struct hermit_config *hermit_config_load(const char *path) {
    struct hermit_config *config = calloc(1, sizeof(*config));
    if (!config) return NULL;
    config_set_defaults(config);

    FILE *f = fopen(path, "r");
    if (!f) {
        wlr_log(WLR_ERROR, "Could not open config: %s, using defaults", path);
        return config;
    }

    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *s = trim(line);

        if (*s == '\0' || *s == '#') continue;

        char *eq = strchr(s, '=');
        if (!eq) {
            wlr_log(WLR_ERROR, "Line %d: missing '='", lineno);
            continue;
        }

        *eq = '\0';
        char *key = trim(s);
        char *val = trim(eq + 1);
        parse_line(config, key, val);
    }

    fclose(f);
    wlr_log(WLR_INFO, "Config loaded from %s (%d keybinds, %d float rules)",
        path, config->keybind_count, config->float_rule_count);
    return config;
}

void hermit_config_destroy(struct hermit_config *config) {
    free(config);
}