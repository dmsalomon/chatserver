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

void comm(int client_fd);

const char *name;

int main(int argc, char **argv)
{
	int sfd, cfd;
	char *addr = DEFADDR;
	unsigned short port = DEFPORT;
	struct sockaddr_in peer_addr;

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

	setbuf(stdout, NULL);

	memset(&peer_addr, 0, sizeof(peer_addr));
	peer_addr.sin_family = AF_INET;
	peer_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, addr, &peer_addr.sin_addr) == 0)
		die(1, "%s: not a valid ip address", addr);

	cfd = socket(PF_INET, SOCK_STREAM, 0);
	if (cfd < 0)
		pdie(1, "socket()");

	printf(HEADER "connecting...");
	sfd = connect(cfd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
	puts("done");

	if (sfd < 0)
		pdie(1, "connect()");

	comm(cfd);

	close(sfd);
	close(cfd);
}

/* communicate with the server
 *
 * The client doesn't ignore SIGPIPE, so it
 * will crash if the server is unreachable. This
 * is expected behavior, since the client is ephemeral.
 */
void comm(int cfd)
{
	int n;
	char sender[NAMESIZE];
	char buf[BUFSIZE];

	/* sending name */
	write(cfd, name, strlen(name));
	n = write(cfd, "\n", 1);

	// is the server already gone?
	if (n == 0)
		return;

	fd_set rfds;
	FD_ZERO(&rfds);

	for (;;) {
		FD_SET(0, &rfds);
		FD_SET(cfd, &rfds);

		if (select(cfd + 1, &rfds, NULL, NULL, NULL) < 0)
			pdie(1, "select()");

		if (FD_ISSET(0, &rfds)) {
			n = readline(0, buf, sizeof(buf));

			// control-D
			if (n == 0)
				break;

			n = write(cfd, buf, n);
			if (n == -1)
				break;
		}

		if (FD_ISSET(cfd, &rfds)) {
			if (read_line(cfd, sender, NAMESIZE) < 1)
				break;
			if (read_line(cfd, buf, BUFSIZE) < 1)
				break;

			printf("[%s] %s\n", sender, buf);
		}
	}
}
