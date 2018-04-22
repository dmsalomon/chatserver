/* \file client.c
 *
 * Dov Salomon (dms833)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PROGNAME "client"
#include "util.h"

int tcpopen(const char *addr, unsigned short port);
void comm(int client_fd);

const char *name;

int main(int argc, char **argv)
{
	int fd;
	char *addr = DEFADDR;
	unsigned short port = DEFPORT;

	if (argc < 2 || argc > 4)
		die(1, "usage: " PROGNAME " username [host] [port]");

	name = argv[1];

	if (strlen(name) > NAMESIZE - 1)
		die(1, "username cannot exceed %d characters", NAMESIZE - 1);

	if (argc > 2)
		addr = argv[2];
	if (argc > 3)
		port = atoport(argv[3]);

	if (port == 0)
		die(1, "invalid port number");


	fd = tcpopen(addr, port);
	setbuf(stdout, NULL);

	comm(fd);
	close(fd);
}

int tcpopen(const char *addr, unsigned short port)
{
	int fd;
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	if (inet_pton(AF_INET, addr, &sin.sin_addr) == 0)
		die(1, "%s: not a valid ip address", addr);

	if ((fd= socket(PF_INET, SOCK_STREAM, 0)) < 0)
		pdie(1, "socket()");

	if(connect(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		pdie(1, "connect()");

	return fd;

}


/* communicate with the server
 *
 * The client doesn't ignore SIGPIPE, so it
 * will crash if the server is unreachable. This
 * is expected behavior, since the client is ephemeral.
 */
void comm(int fd)
{
	int n;
	char sender[NAMESIZE];
	char buf[BUFSIZE];

	/* sending name */
	write(fd, name, strlen(name));
	n = write(fd, "\n", 1);

	// is the server already gone?
	if (n == 0)
		return;

	fd_set rfds;
	FD_ZERO(&rfds);

	for (;;) {
		FD_SET(0, &rfds);
		FD_SET(fd, &rfds);

		if (select(fd + 1, &rfds, NULL, NULL, NULL) < 0)
			pdie(1, "select()");

		if (FD_ISSET(0, &rfds)) {
			n = read_line(0, buf, sizeof(buf));

			// control-D
			if (n == 0)
				break;

			buf[n - 1] = '\n';

			n = write(fd, buf, n);
			if (n == -1)
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
}
