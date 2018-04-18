/* \file server.c
 *
 * Dov Salomon (dms833)
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PROGNAME "server"
#include "util.h"

struct client {
	int fd;
	char name[NAMESIZE];
	struct client *next;
};

struct msg {
	char *buf;
	struct client *sender;
	struct msg *next;
};

int tcpopen(unsigned short port);
void *serve(void *);
void *poll(void *);
void *hog(void *);
void *broadcast(void *);
void handler(int sig);
void loginfo(const char *, ...);

struct msg *msg_add(struct client *, const char *);
void msg_rm(struct msg *);
void msg_send(struct msg *);

struct client *client_add(int, const char *);
void client_rm(struct client *);

struct msg *msgs;
pthread_mutex_t msg_mx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t msg_has = PTHREAD_COND_INITIALIZER;

struct client *clients;
pthread_mutex_t client_mx = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv)
{
	int sfd, cfd;
	unsigned short port = DEFPORT;
	struct sockaddr_in peer_addr;
	socklen_t peer_sz = sizeof(peer_addr);
	pthread_t tid;

	if (argc > 2)
		die(1, "usage: server [port]");

	if (argc > 1)
		port = atoport(argv[2]);

	if (port == 0)
		die(1, "invalid port number");

	setbuf(stdout, NULL);
	signal(SIGPIPE, SIG_IGN);

	sfd = tcpopen(port);
	//pthread_create(&tid, NULL, poll, NULL);
	pthread_create(&tid, NULL, broadcast, NULL);

	// continuously accept clients
	while ((cfd =
		accept(sfd, (struct sockaddr *)&peer_addr, &peer_sz)) != -1) {
		if (pthread_create(&tid, NULL, serve, &cfd))
			pdie(1, "pthread_create()");
	}

	// error if here
	pdie(1, "accept()");
	close(sfd);
}

void *serve(void *arg)
{
	char name[NAMESIZE];
	char buf[BUFSIZE];
	struct client *c;

	if (!arg)
		return NULL;

	int cfd = *(int *)arg;

	if (cfd <= 0)
		return NULL;

	if (read_line(cfd, name, NAMESIZE) < 0)
		return NULL;

	c = client_add(cfd, name);
	sprintf(buf, "%s has entered", name);
	msg_add(c, buf);
	loginfo("receieved client: %s\n", name);

	while (read_line(cfd, buf, BUFSIZE) > 0) {
		loginfo("msg: [%s] %s\n", name, buf);
		msg_add(c, buf);
	}

	close(cfd);
	sprintf(buf, "%s has left", name);
	msg_add(c, buf);
	loginfo("done with client :(\n");

	client_rm(c);
	return NULL;
}


void *broadcast(void *arg)
{
	struct msg *m;

	pthread_mutex_lock(&msg_mx);

	for (;;) {
		for (m = msgs; m; m = m->next) {
			msg_send(m);
			msg_rm(m);
		}
		pthread_cond_wait(&msg_has, &msg_mx);
	}

	return NULL;
}

void msg_send(struct msg *m)
{
	struct client *c;

	pthread_mutex_lock(&client_mx);

	for (c = clients; c; c = c->next) {
		if (c == m->sender)
			continue;

		write(c->fd, m->sender->name, strlen(m->sender->name));
		write(c->fd, "\n", 1);
		write(c->fd, m->buf, strlen(m->buf));
		write(c->fd, "\n", 1);
	}

	pthread_mutex_unlock(&client_mx);
}

void *hog(void *arg)
{
	for (;;) {
		pthread_mutex_lock(&client_mx);
		sleep(10);
		pthread_mutex_unlock(&client_mx);
		sleep(2);
	}
	return NULL;
}

void *poll(void *arg)
{
	for (;;) {
		pthread_mutex_lock(&client_mx);
		struct client *p;

		loginfo("poll:\n");
		for (p = clients; p; p = p->next) {
			loginfo("client [%s] [%d]\n", p->name, p->fd);
		}
		pthread_mutex_lock(&msg_mx);
		struct msg *m;
		for (m = msgs; m; m = m->next)
			loginfo("msg: [%s] %s\n", m->sender->name, m->buf);
		loginfo("done\n");
		pthread_mutex_unlock(&client_mx);
		pthread_mutex_unlock(&msg_mx);
		sleep(3);
	}
	return NULL;
}


void loginfo(const char *fmt, ...)
{
	static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

	va_list ap;
	va_start(ap, fmt);

	pthread_mutex_lock(&m);
	vprintf(fmt, ap);
	pthread_mutex_unlock(&m);

	va_end(ap);
}

struct msg *msg_add(struct client *sender, const char *buf)
{
	struct msg *p, *m = dmalloc(sizeof(struct msg));
	m->buf = strdup(buf);
	m->sender = sender;

	pthread_mutex_lock(&msg_mx);

	for (p = msgs; p && p->next; p = p->next)
		;

	if (p)
		p->next = m;
	else
		msgs = m;

	m->next = NULL;

	pthread_mutex_unlock(&msg_mx);
	pthread_cond_broadcast(&msg_has);

	return m;
}

void msg_rm(struct msg *m)
{
	struct msg *p;

	if (msgs == m) {
		msgs = msgs->next;
	} else {
		for (p = msgs; p && p ->next != m; p = p->next)
			;
		if (p && p->next == m)
			p->next = m->next;
	}
	free(m->buf);
	free(m);
}

struct client *client_add(int cfd, const char *name)
{
	struct client *c = dmalloc(sizeof(struct client));
	c->fd = cfd;
	strncpy(c->name, name, strlen(name));

	pthread_mutex_lock(&client_mx);

	if (clients)
		c->next = clients;
	else
		c->next = NULL;
	clients = c;

	pthread_mutex_unlock(&client_mx);

	return c;
}

void client_rm(struct client *c)
{
	struct client *p;

	pthread_mutex_lock(&client_mx);

	if (clients == c) {
		clients = clients->next;
	} else {
		for (p = clients; p && p->next != c; p = p->next)
			;
		if (p && p->next == c)
			p->next = c->next;
	}
	free(c);

	pthread_mutex_unlock(&client_mx);
}

/*
 * allocate the socket and bind to port
 */
int tcpopen(unsigned short port)
{
	int sfd;
	struct sockaddr_in my_addr;

	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port);
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
