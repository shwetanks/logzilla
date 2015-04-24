#include "logzilla.h"
#include "logdaemon_config.h"
#include "logger.h"
#include "logzilla_defs.h"

#include "logz.h"

struct logdaemon_config logconf = LOGDAEMON_INITIALIZER;

#define MAX_FILE_SUPPORT 10

const char *watch_dir = NULL;

int main(int argc, char *argv[]) {

    size_t num_files = 0;

    char **files = (char **) calloc(MAX_FILE_SUPPORT, MAX_FILE_SUPPORT * sizeof(char *));

    if (0 > init_log_config(&logconf, argc, argv)) {
        exit(EXIT_FAILURE);
    }
    char *f = logconf.watch_files;
    if (!STRING_ISEMPTY(f)) {
        while (f != NULL) {
            char *fprime = strsep(&f, ",");
            if (fprime != NULL) {
                files[num_files] = strdup(fprime);
                ++num_files;
            }
        }
    } else {
        LOG_ERROR("%s", "no files..no watch!");
        exit(EXIT_FAILURE);
    }

    attach_observe_events (num_files, files, logconf.target);
    free(files);
    return 0;
}
