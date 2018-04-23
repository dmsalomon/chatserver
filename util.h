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

#define BUFSIZE 1024
#define NAMESIZE 32
#define DEFPORT 2000
#define DEFADDR "127.0.0.1"
#define HEADER "[" PROGNAME "] "

void perrorf(const char *fmt, ...);
void reportf(const char *fmt, ...);
void die(int status, const char *fmt, ...) __attribute__ ((noreturn));
void pdie(int status, const char *fmt, ...) __attribute__ ((noreturn));
pid_t dfork(void);
void *dmalloc(size_t);

void vperrorf(const char *fmt, va_list ap)
{
	fprintf(stderr, PROGNAME ": ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", strerror(errno));
}

/*
 * Like perror but with formatting
 */
void perrorf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vperrorf(fmt, ap);
	va_end(ap);
}

/*
 * perrorf and then exit
 */
void pdie(int status, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vperrorf(fmt, ap);
	va_end(ap);

	exit(status);
}

void vreportf(const char *fmt, va_list ap)
{
	fprintf(stderr, PROGNAME ": ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

/*
 * report and error with formatting
 */
void reportf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vreportf(fmt, ap);
	va_end(ap);
}

void die(int status, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vreportf(fmt, ap);
	va_end(ap);

	exit(status);
}

void *dmalloc(size_t n)
{
	void *p = malloc(n);

	if (!p)
		pdie(1, "malloc()");

	return p;
}

int read_line(int fd, char *buf, size_t bfsz)
{
	int n;
	size_t i = 0;
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
	buf[i - 1] = '\0';
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
