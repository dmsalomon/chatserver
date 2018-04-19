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

void *serve(void *);
void *poll(void *);
void *broadcast(void *);
void loginfo(const char *, ...);
int tcpbind(unsigned short port);

struct client {
	int fd;
	char name[NAMESIZE];
	struct client *next;
};

const struct client _srv = {0, "server", NULL};
const struct client *srv = &_srv;

struct msg {
	char *buf;
	const struct client *sender;
	struct msg *next;
};

struct msg *msg_add(const struct client *, const char *);
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
	int *arg;
	unsigned short port = DEFPORT;
	pthread_t tid;
	struct sockaddr_in peer_addr;
	socklen_t peer_sz = sizeof(peer_addr);

	if (argc > 2)
		die(1, "usage: server [port]");

	if (argc > 1)
		port = atoport(argv[2]);

	if (port == 0)
		die(1, "invalid port number");

	setbuf(stdout, NULL);
	signal(SIGPIPE, SIG_IGN);

	sfd = tcpbind(port);
	//pthread_create(&tid, NULL, poll, NULL);
	if ((errno = pthread_create(&tid, NULL, broadcast, NULL)))
		pdie(1, "pthread_create()");
	if ((errno = pthread_detach(tid)))
		pdie(1, "pthread_detach()");

	// continuously accept clients
	while ((cfd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_sz)) != -1) {
		arg = dmalloc(sizeof(int));
		*arg = cfd;
		if ((errno = pthread_create(&tid, NULL, serve, arg)))
			pdie(1, "pthread_create()");
		if ((errno = pthread_detach(tid)))
			pdie(1, "pthread_detach()");
	}

	// error if here
	pdie(1, "accept()");
	close(sfd); //useless
}

// per client thread
void *serve(void *arg)
{
	int fd;
	char name[NAMESIZE];
	char buf[BUFSIZE];
	struct client *c;

	if (!arg || (fd = *(int *)arg) <= 0)
		return NULL;

	free(arg);

	// get clients name
	if (read_line(fd, name, NAMESIZE) < 0)
		return NULL;

	snprintf(buf, BUFSIZE, "%s has entered", name);
	/* TODO: Make this not trigger on the this client */
	msg_add(srv, buf);
	c = client_add(fd, name);

	while (read_line(fd, buf, BUFSIZE) > 0) {
		loginfo("msg: [%s] %s\n", name, buf);
		msg_add(c, buf);
	}

	client_rm(c);
	close(fd);

	snprintf(buf, BUFSIZE, "%s has left", name);
	msg_add(srv, buf);
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

	if (m->sender == srv)
		loginfo("[%s] %s\n", srv->name, m->buf);
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

struct msg *msg_add(const struct client *sender, const char *buf)
{
	struct msg *p, *m = dmalloc(sizeof(struct msg));
	m->buf = strdup(buf);
	if (!m->buf) pthread_exit(NULL);
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

struct client *client_add(int fd, const char *name)
{
	struct client *c = dmalloc(sizeof(struct client));
	c->fd = fd;
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
int tcpbind(unsigned short port)
{
	int fd;
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		pdie(1, "socket()");

	/* Make the port immediately reusable after termination */
	int reuse = 1;
	if (setsockopt
	    (fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse,
	     sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEADDR) failed");

#ifdef SO_REUSEPORT
	if (setsockopt
	    (fd, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse,
	     sizeof(reuse)) < 0)
		perror("setsockopt(SO_REUSEPORT) failed");
#endif

	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0)
		pdie(1, "bind()");

	if (listen(fd, 50) < 0)
		pdie(1, "listen()");

	return fd;
}
