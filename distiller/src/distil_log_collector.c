#include "distil_log_collector.h"

extern struct distiller_config ds_conf;

int main (int argc, char* argv[]) {

    init_distiller_config(argc, argv);
    if (SSTRISEMPTY(ds_conf.file_source) && SSTRISEMPTY(ds_conf.nw_source->hostname)) {
        LOGGER_ERROR("%s", "requires either of two input sources");
        exit (EXIT_FAILURE);
    }

    
    
    return 0;
}
    
