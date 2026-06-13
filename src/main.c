#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <wlr/util/log.h>
#include "hermit/server.h"
#include "hermit/logger.h"


int main(int argc, char *argv[]) {
    // Check args
    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-debug") == 0 || strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0)
            hlog_setDebug(true);
        else
            hlog_setDebug(false);
    }
    hlog_debug("Debug logs enabled.");
    hlog_info("Welcome to Hermit! If you encounter any bugs, please create an Issue on Github :)");
    wlr_log_init(WLR_ERROR, NULL);
    
    struct hermit_server server = {0};
    
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.config/hermit/hermit-comp.conf", getenv("HOME"));
    server.config = hermit_config_load(config_path);
    if (!server.config) {
        hlog_error("Failed to load config");
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