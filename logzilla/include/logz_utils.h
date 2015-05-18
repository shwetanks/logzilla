#ifndef _LOGZ_UTILS_H_
#define _LOGZ_UTILS_H_

#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>

int
resolve_hostname(
    const char *hostname,
    struct in_addr *addr) {

    struct hostent hent;
    int herror;
    char buffer[16384];
    struct hostent *h;
    int res = gethostbyname_r(hostname, &hent, buffer, sizeof(buffer), &h, &herror);
    if (0 != res || NULL == h || NULL == (struct in_addr *)h->h_addr_list)
        return -1;
    *addr = *(struct in_addr *)h->h_addr_list[0];
    return 0;
}

int
parse_host_to_inet (
    const char *host,
    char *hostname,
    struct in_addr *addr,
    uint16_t *port) {

    char buffer[strlen(host) + 1];
    strcpy(buffer, host);

    char * token;
    char *sp;

    token = strtok_r(buffer, ":", &sp);
    if (token != NULL) {
        strcpy(hostname, token);
    }

    token = strtok_r(NULL, ":", &sp);
    if (token != NULL) {
        *port = atoi(token);
    } else {
        *port = 80;
    }

    if (0 > resolve_hostname(buffer, addr)) {
        return -1;
    }
    return 0;
}


void
_replace(
    char *in,
    struct vmbuf *out,
    const char *find,
    const char *replace) {

    char *fstart;

    vmbuf_reset(out);
    if(!(fstart = strstr(in, find))) {
        vmbuf_strcpy(out, in);
        return;
    }
    char *remainder = ribs_strdup(fstart + strlen(find));

    vmbuf_memcpy(out, in, fstart-in);
    vmbuf_sprintf(out, "%s%s", replace, remainder);
    return;
}


#endif /* _LOGZ_UTILS_H_ */
