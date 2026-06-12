#include <stdlib.h>
#include <signal.h>
#include <wlr/util/log.h>
#include "hermit/server.h"

static struct hermit_server *g_server = NULL;

static void handle_signal(int sig) {
    if (g_server) {
        wl_display_terminate(g_server->display);
    }
}

int main(int argc, char *argv[]) {
    wlr_log_init(WLR_INFO, NULL);
    
    struct hermit_server server = {0};
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.config/hermit/hermit-comp.conf", getenv("HOME"));
    server.config = hermit_config_load(config_path);
    if (!server.config) {
        wlr_log(WLR_ERROR, "Failed to load config");
        return 1;
    }
    
    if (!hermit_server_init(&server)) {
        return 1;
    }
    
    hermit_server_run(&server);
    hermit_server_end(&server);
    hermit_config_destroy(server.config);
    return 0;
}