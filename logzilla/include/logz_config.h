#ifndef _LOGZ_CONFIG_H_
#define _LOGZ_CONFIG_H_

#include "ribs.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "logz_struct_defs.h"

#define LOGDAEMON_INITIALIZER {NULL, NULL, NULL, NULL}

struct logdaemon_config {
    char *watch_files;
    char *exclude_files;
    char *target;
    char *interface;
};

void
usage(char *arg0) {
    printf("\nlogzilla v1.0: file content spooling utility\n\n");

    printf("usage: \n");
    printf("       %*c  [-f|--files] required(files to watch) supports comma delimited names \n", (int)strlen(arg0), ' ');
    printf("       %*c  [-E|--exclude-like] optional(exclude such files) | not implemented \n", (int)strlen(arg0), ' ');
    printf("       %*c  [-t|--target]  optional(create/append-write to this target file)\n", (int)strlen(arg0), ' ');
    printf("       %*c  [-s|--write-to]  optional(receive collated data on this HTTP interface. logs to target otherwise. One of them is required)\n", (int)strlen(arg0), ' ');
    printf("       %*c  [--help] prints this help\n", (int)strlen(arg0), ' ');
    printf("\n");

    exit(EXIT_FAILURE);
}


static inline int
init_log_config (struct logdaemon_config *config, int argc, char *argv[]) {

    static struct option longopts[] = {
        {"files", 1, 0, 'f'},
        {"exclude-like", 2, 0, 'E'},
        {"target", 2, 0, 't'},
        {"write-to", 2, 0, 's'},
        {"help", 0, 0, 1},
        {0, 0, 0, 0}
    };

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "f:t:s:E::", longopts, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'f':
            config->watch_files = strdup(optarg);
            break;
        case 't':
            config->target = strdup(optarg);
            break;
        case 'E':
            config->exclude_files = strdup(optarg);
            break;
        case 's':
            config->interface = strdup(optarg);
            break;
        default:
            usage(argv[0]);
            break;
        }
    }
    return 0;
}

#endif /* _LOGZ_CONFIG_H_ */
