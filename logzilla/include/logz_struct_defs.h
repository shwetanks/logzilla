#ifndef _LOGZ_STRUCT_DEFS_H_
#define _LOGZ_STRUCT_DEFS_H_

struct server {
    char hostname[128];
    struct in_addr server;
    uint16_t port;
    char *context;
};


#endif /* _LOGZ_STRUCT_DEFS_H_ */
