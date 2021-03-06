/* \file client.c
 *
 * Dov Salomon (dms833)
 */

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PROGNAME "client"
#include "util.h"

int tcpopen(const char *host, const char *serv);
void comm(int client_fd);

const char *name;
char *argv0 = "client";

int main(int argc, char **argv)
{
	int fd;
	const char *host = DEFADDR;
	const char *port = DEFPORT;

	if (argc < 2 || argc > 4)
		die("usage: %s username [host] [port]", argv[0]);

	name = argv[1];

	if (strlen(name) > NAMESIZE - 1)
		die("username cannot exceed %d characters", NAMESIZE - 1);

	if (argc > 2)
		host = argv[2];
	if (argc > 3)
		port = argv[3];

	setbuf(stdout, NULL);

	/* ignore SIGPIPE */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		die("ignoring SIGPIPE:");

	fd = tcpopen(host, port);
	printf("[%s] established connection to %s:%s\n",
			PROGNAME, host, port);

	comm(fd);
	close(fd);
}


/* main communication routine */
void comm(int fd)
{
	int n;
	char sender[NAMESIZE];
	char buf[BUFSIZE];

	fd_set rfds;
	FD_ZERO(&rfds);

	/* sending name */
	write(fd, name, strlen(name));
	n = write(fd, "\n", 1);

	while (n > 0) {
		FD_SET(0, &rfds);
		FD_SET(fd, &rfds);

		if (select(fd + 1, &rfds, NULL, NULL, NULL) < 0)
			die("select():");

		if (FD_ISSET(0, &rfds)) {
			n = read_line(0, buf, sizeof(buf)-1);

			// control-D or error
			if (n < 1)
				break;

			if (strcmp(buf, "/q") == 0)
				break;

			buf[n-1] = '\n';
			n = write(fd, buf, n);

			if (n < 0)
				break;
		}

		if (FD_ISSET(fd, &rfds)) {
			if (read_line(fd, sender, NAMESIZE) < 1)
				break;
			if (read_line(fd, buf, BUFSIZE) < 1)
				break;

			printf("[%s] %s\n", sender, buf);
		}
	}

	if (n < 0) perror(PROGNAME);
}

/* connects a TCP socket to the host
 *
 * Host can either be an ip address or
 * a domain name. Port can either be a
 * numeric port, or a service name
 */
int tcpopen(const char *host, const char *port)
{
	struct addrinfo hints, *res = NULL, *rp;
	int fd = -1, e;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_socktype = SOCK_STREAM;

	if ((e = getaddrinfo(host, port, &hints, &res)))
		die("getaddrinfo: %s", gai_strerror(e));

	for (rp = res; rp; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

		if (fd < 0)
			continue;

		if (connect(fd, rp->ai_addr, rp->ai_addrlen) < 0) {
			close(fd);
			fd = -1;
			continue;
		}
		break;
	}

	freeaddrinfo(res);

	if (fd < 0)
		die("could not connect to %s:%s:", host, port);

	return fd;
}

