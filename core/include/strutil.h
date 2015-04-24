#ifndef _STRUTIL__H_
#define _STRUTIL__H_

#include <string.h>

#define STRING(name, value) \
    const char name[] = value

#define EXTERN_STRING(name, value) \
    extern const char name[] = value

#define STRING_LOCAL(name, value) \
    static const char name[] = value

#define STRING_LEN(var) \
    (sizeof(var) - 1)

#define STRING_CMP(var, with) \
    (strncmp(var, with, STRING_LEN(var)))

#define STRING_REGION_IGN_CMP(var, with) \
    (strncasecmp(var, with, STRING_LEN(var)))

#define STRING_REGION_CMP(var, with) \
    (strncmp(var, with, sizeof(var)))

#define STRING_ISEMPTY(var) \
    ((var == NULL)||(var[0]=='\0'))

#endif // _STRUTIL__H_
