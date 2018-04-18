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


/* Taken from LPI sockets/read_line.c pg. 1201
 * Michael Kerrisk
 * slightly modified.
 *
 * Reads a line from fd.
 */
int readline(int fd, char *buf, int n)
{
	int ln;
	int tn = 0;
	char c;

	if (n <= 0 || !buf) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		ln = read(fd, &c, 1);

		if (ln == -1) {
			if (errno == EINTR)
				continue;
			else
				return -1;
		} else if (ln == 0) {
			if (tn == 0)
				return 0;
			else
				break;
		} else {
			if (tn < n-1) {
				tn++;
				*buf++ = c;
			}

			if (c == '\n' || tn == n-1)
				break;
		}
	}

	return tn;
}

int read_line(int fd, char *buf, size_t bfsz)
{
	size_t i = 0;
	char c = '\0';

	do {
		if (read(fd, &c, sizeof(char)) != sizeof(char))
			return -1;
		buf[i++] = c;
	} while (c != '\n' && i < bfsz);
	buf[i - 1] = '\0';
	return i;
}

/* Taken from LPI sockets/read_line.c pg. 1201
 * Michael Kerrisk
 * slightly modified.
 *
 * Reads a line from fd.
 */
int readuntil(int fd, char *buf, int n, char until)
{
	int ln;
	int tn = 0;
	char c;

	if (n <= 0 || !buf) {
		errno = EINVAL;
		return -1;
	}

	for (;;) {
		ln = read(fd, &c, 1);

		if (ln == -1) {
			if (errno == EINTR)
				continue;
			else
				return -1;
		} else if (ln == 0) {
			if (tn == 0)
				return 0;
			else
				break;
		} else {
			if (tn < n) {
				tn++;
				*buf++ = c;
			}

			if (c == until || tn == n)
				break;
		}
	}

	return tn;
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
