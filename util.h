/** \file util.h
 * Provides some useful utility functions
 *
 * Dov Salomon (dms833)
 */

#ifndef UTIL_H
#define UTIL_H

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PROGNAME
#warning PROGNAME undefined
#define PROGNAME "prog"
#endif

#define STR(x)        STR_HELPER(x)
#define STR_HELPER(x) #x
#define STRLEN(s)     (sizeof(s)/sizeof(s[0]) - sizeof(s[0]))

#define BUFSIZE  4096
#define NAMESIZE 32
#define DEFADDR  "127.0.0.1"
#define DEFPORT  2000
#define DEFSERV  STR(DEFPORT)

void die(const char *fmt, ...) __attribute__ ((noreturn));
void *dmalloc(size_t);
int fdprintf(int fd, const char *fmt, ...);

void die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, PROGNAME ": ");
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

void *dmalloc(size_t n)
{
	void *p;

	if (!(p = malloc(n)))
		die("malloc():");

	return p;
}

int read_line(int fd, char *buf, size_t bfsz)
{
	int n;
	int i = 0;
	char c = '\0';

	do {
		n = read(fd, &c, sizeof(char));
		if (n == -1)
			if (errno == EINTR)
				continue;
			else
				return -1;
		else if (n == 0)
			if (i)
				break;
			else
				return 0;
		else
			buf[i++] = c;
	} while (c != '\n' && i < bfsz);
	buf[i-1] = '\0';
	return i;
}

/* converts port number to ushort
 *
 * returns zero on error
 * (we won't use port "0")
 */
unsigned short atoport(char *str)
{
	char *end;
	unsigned long int port;

	port = strtoul(str, &end, 10);

	// not a valid string
	if (*end != '\0')
		return 0;

	return port;
}

#endif
