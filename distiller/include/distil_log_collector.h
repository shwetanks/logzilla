#ifndef _DISTIL_LOG_COLLECTOR_
#define _DISTIL_LOG_COLLECTOR_

#include "ribs.h"
#include <getopt.h>

#include "logz_utils.h"
#include "logz_struct_defs.h"

struct distiller_config {
    char *file_source;
    char *nw_uri_context;
    char *data_dir;
    struct server *nw_source;
    struct vmbuf tmp;
};

struct distiller_config ds_conf;

void usage(char *arg0) {
    printf("\nDistiller v1.0: Data Extractor and Synthetic Analyzer\n\n");

    printf("usage: \n");
    printf("       %*c  [-f|--fl-source]  optional(file data-source)\n", (int)strlen(arg0), ' ');
    printf("       %*c  [-s|--nw-source]  optional(network data-source. A typical HTTP server URI data-endpoint)\n", (int)strlen(arg0), ' ');
    printf("       %*c  [-d|--data]  required(dump to this directory)\n", (int)strlen(arg0), ' ');
    printf("       %*c  [--help] prints this help\n", (int)strlen(arg0), ' ');
    printf("\n");

    exit(EXIT_FAILURE);
}


static inline int 
init_distiller_config (int argc, char *argv[]) {

    memset(&ds_conf, 0, sizeof(ds_conf));
    vmbuf_init(&ds_conf.tmp, 4096);
  
    static struct option longopts[] = {
        {"fl-source", 2, 0, 'f'},
        {"nw-source", 2, 0, 's'},
        {"data", 1, 0, 'd'},
        {"help", 0, 0, 1},
        {0, 0, 0, 0}
    };

    while (1) {
        int option_index = 0;
        int c = getopt_long(argc, argv, "f:s:d", longopts, &option_index);
        if (c == -1)
            break;
        switch (c) {
        case 'f':
            ds_conf.file_source = strdup(optarg);
            break;
        case 'd':
            ds_conf.data_dir = strdup(optarg);
            break;
        case 's':
            vmbuf_reset(&ds_conf.tmp);
            char *uri = strdup(optarg);
            _replace(uri, &ds_conf.tmp, "http://www.", "");
            _replace(uri, &ds_conf.tmp, "http://", "");

            char *interface = vmbuf_data(&ds_conf.tmp);
            ds_conf.nw_uri_context = strdup(strstr(interface, "/"));
        
            char *ln = strchr(interface, '/');
            ln = ribs_malloc_sprintf("%.*s", ((int)strlen(interface) - (int)strlen(ln)), interface);

            if (0 > parse_host_to_inet(ln, ds_conf.nw_source->hostname, &ds_conf.nw_source->server, &ds_conf.nw_source->port)) {
                LOGGER_ERROR("%s", "server details invalid. cannot parse server");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            usage(argv[0]);
            break;
        }
    }
    return 0;
}

#endif /* _DISTIL_LOG_COLLECTOR_ */
