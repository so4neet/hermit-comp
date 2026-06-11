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
    wlr_log_init(WLR_DEBUG, NULL);
    
    struct hermit_server server = {0};
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    if (!hermit_server_init(&server)) {
        return 1;
    }
    
    hermit_server_run(&server);
    hermit_server_end(&server);
    
    return 0;
}