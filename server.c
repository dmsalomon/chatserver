/* \file server.c
 *
 * Dov Salomon (dms833)
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PROGNAME "server"
#include "util.h"

int init(unsigned short port);
void serve(int cfd);
void handler(int sig);

int main(int argc, char **argv)
{
	int sfd, cfd;
	unsigned short port = DEFPORT;
	struct sockaddr_in peer_addr;
	socklen_t peer_sz = sizeof(peer_addr);

	if (argc < 2 || argc > 3)
		die(1, "usage: server username [port]");

	name = argv[1];

	if (strlen(name) > NAMESIZE - 1)
		die(1, "username cannot exceed %d characters", NAMESIZE - 1);

	if (argc > 2)
		port = atoport(argv[2]);

	if (port == 0)
		die(1, "invalid port number");

	setbuf(stdout, NULL);
	signal(SIGPIPE, SIG_IGN);

	sfd = init(port);

	// continuously accept clients
	while ((cfd =
		accept(sfd, (struct sockaddr *)&peer_addr, &peer_sz)) != -1) {
		serve(cfd);
	}

	// error if here
	pdie(1, "accept()");
	close(sfd);
}

/*
 * allocate the socket and bind to port
 */
int init(unsigned short port)
{
	int sfd;
	struct sockaddr_in my_addr;

	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
	// bind to any address
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	sfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sfd < 0)
		pdie(1, "socket()");

	/* Make the port immediately reusable after termination */
	int reuse = 1;
	if (setsockopt
	    (sfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
	     sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
	if (setsockopt
	    (sfd, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse,
	     sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEPORT) failed");
#endif

	if (bind(sfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0)
		pdie(1, "bind()");

	if (listen(sfd, 50) < 0)
		pdie(1, "listen()");

	return sfd;
}
