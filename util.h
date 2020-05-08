/** \file util.h
 * Provides some useful utility functions
 *
 * Dov Salomon (dms833)
 */

#ifndef UTIL_H
#define UTIL_H

#define STRLEN(s)     (sizeof(s)/sizeof(s[0]) - sizeof(s[0]))

#define BUFSIZE  512
#define NAMESIZE 32
#define DEFADDR  "localhost"
#define DEFPORT  "2000"

void die(const char *fmt, ...) __attribute__ ((noreturn));
void *xmalloc(size_t);
int read_line(int, char *, size_t);
unsigned short atoport(const char *);

#endif
