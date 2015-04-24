#ifndef _LOGDAEMON_CONFIG_H_
#define _LOGDAEMON_CONFIG_H_

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define LOGDAEMON_INITIALIZER {NULL, NULL, NULL}

struct logdaemon_config {
    char *watch_files;
    char *exclude_files;
    char *target;
    //TODO introduce priority_includes
};

void
usage(char *arg0) {
    printf("\nlogzilla v1.0: file content spooling utility\n\n");

    printf("usage: \n");
    printf("       %*c  [-f|--files] required(files to watch) supports comma delimited names \n", (int)strlen(arg0), ' ');
    printf("       %*c  [-E|--exclude-like] optional(exclude such files) | not implemented \n", (int)strlen(arg0), ' ');
    printf("       %*c  [-t|--target]  required(create/append-write to this target file)\n", (int)strlen(arg0), ' ');
    printf("       %*c  [-e|--events]  optional(listen to these events. defaults to MODIFY)\n", (int)strlen(arg0), ' ');
    printf("       %*c  [--help] prints this help\n", (int)strlen(arg0), ' ');
    printf("\n");

    exit(EXIT_FAILURE);
}


static inline int
init_log_config (struct logdaemon_config *config, int argc, char *argv[]) {

    static struct option longopts[] = {
        {"files", 1, 0, 'f'},
        {"exclude-like", 2, 0, 'E'},
        {"target", 1, 0, 't'},
//        {"events", 2, 0, 'e'},
        {"help", 0, 0, 1},
        {0, 0, 0, 0}
    };

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "f:t:E::", longopts, &option_index);
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
        default:
            usage(argv[0]);
            break;
        }
    }
    return 0;
}

#endif //_LOGDAEMON_CONFIG_H_
