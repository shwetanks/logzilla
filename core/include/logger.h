#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <stdarg.h>

void zlog(const char *format, ...) __attribute__ ((format (gnu_printf, 1, 2)));
void zerror_at_line(const char *filename, unsigned int linenum, const char *format, ...) __attribute__ ((format (gnu_printf, 3, 4)));
void zperror_at_line(const char *filename, unsigned int linenum, const char *format, ...) __attribute__ ((format (gnu_printf, 3, 4)));

#define LOG_WHERE __FILE__, __LINE__
#define LOG_INFO(format, ...) zlog(format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) zerror_at_line(LOG_WHERE, format, ##__VA_ARGS__)
#define LOG_PERROR(format, ...) zperror_at_line(LOG_WHERE, format, ##__VA_ARGS__)

#endif /*_LOGGER_H_*/
