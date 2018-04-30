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
void *broadcast(void *);
int tcpbind(unsigned short port);

struct client {
	int fd;
	char name[NAMESIZE];
	struct client *next;
};

enum msg_type { MSG_REG, MSG_ADM };

struct msg {
	enum msg_type type;
	char buf[BUFSIZE];
	const struct client *sender;
	struct msg *next;
};

struct client *clients;
pthread_mutex_t client_mx = PTHREAD_MUTEX_INITIALIZER;

struct client *client_add(int, const char *);
void client_rm(struct client *);

struct msg *msg_push(enum msg_type, const struct client *, const char *);
struct msg *msg_pop(void);
void msg_send(struct msg *);

struct {
	struct msg *head;
	struct msg *tail;
	pthread_mutex_t mx;
	pthread_cond_t has;
	pthread_cond_t empty;
} msgq = {
	.mx = PTHREAD_MUTEX_INITIALIZER,
	.has = PTHREAD_COND_INITIALIZER,
	.empty = PTHREAD_COND_INITIALIZER
};

void loginfo(const char *, ...);
void logmsg(const struct msg *);

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
		port = atoport(argv[1]);

	if (port == 0)
		die(1, "invalid port number");

	setbuf(stdout, NULL);
	signal(SIGPIPE, SIG_IGN);

	sfd = tcpbind(port);

	// spawn the thread to send messages.
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
void *serve(void *pfd)
{
	int fd;
	char name[NAMESIZE];
	char buf[BUFSIZE];
	struct client *c;

	if (!pfd || (fd = *(int *)pfd) <= 0)
		return NULL;

	free(pfd);

	if (read_line(fd, name, NAMESIZE) < 0)
		return NULL;

	c = client_add(fd, name);
	sprintf(buf, "%s has entered", name);
	msg_push(MSG_ADM, c, buf);

	while (read_line(fd, buf, BUFSIZE) > 0) {
		if (strncmp(buf, "/q", 2) == 0)
			break;
		msg_push(MSG_REG, c, buf);
	}


	sprintf(buf, "%s has left", name);
	msg_push(MSG_ADM, c, buf);

	client_rm(c);
	/* release the file descriptor only after the
	 * client has been removed.
	 */
	close(fd);

	return NULL;
}


void *broadcast(void *arg)
{
	struct msg *m;

	while ((m = msg_pop())) {
		msg_send(m);
		free(m);
	}

	return NULL;
}

void msg_send(struct msg *m)
{
	struct client *c;

	logmsg(m);

	/* Writing to client may cause an EPIPE
	 * since the client may have already quit.
	 * This is safely ignored
	 */

	pthread_mutex_lock(&client_mx);

	for (c = clients; c; c = c->next) {
		if (c == m->sender)
			continue;

		/* TODO: Which write errors do we care about ? */

		if (m->type == MSG_ADM) {
			write(c->fd, PROGNAME, strlen(PROGNAME));
			write(c->fd, "\n", 1);
		}
		else {
			write(c->fd, m->sender->name, strlen(m->sender->name));
			write(c->fd, "\n", 1);
		}

		write(c->fd, m->buf, strlen(m->buf));
		write(c->fd, "\n", 1);
	}

	pthread_mutex_unlock(&client_mx);
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

void logmsg(const struct msg *m)
{
	printf("[%s] %s\n",
		m->type == MSG_ADM ? PROGNAME : m->sender->name,
		m->buf);
}

struct msg *msg_push(enum msg_type type, const struct client *sender, const char *buf)
{
	struct msg *m = dmalloc(sizeof(struct msg));

	m->type = type;
	m->sender = sender;
	strcpy(m->buf, buf);

	pthread_mutex_lock(&msgq.mx);

	if (msgq.tail) {
		msgq.tail->next = m;
		msgq.tail = m;
	} else {
		if (msgq.head)
			die(1, "message queue misalignment");
		msgq.head = msgq.tail = m;
	}

	m->next = NULL;

	pthread_mutex_unlock(&msgq.mx);
	pthread_cond_broadcast(&msgq.has);

	return m;
}

struct msg *msg_pop()
{
	struct msg *p;

	pthread_mutex_lock(&msgq.mx);

	while (!msgq.head) {
		pthread_cond_broadcast(&msgq.empty);
		pthread_cond_wait(&msgq.has, &msgq.mx);
	}

	p = msgq.head;
	msgq.head = (msgq.head)->next;

	if (p == msgq.tail) {
		if (msgq.head)
			die(1, "message queue misalignment");
		msgq.tail = NULL;
	}

	pthread_mutex_unlock(&msgq.mx);

	return p;
}

struct client *client_add(int fd, const char *name)
{
	struct client *c = dmalloc(sizeof(struct client));
	c->fd = fd;
	strcpy(c->name, name);

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

	pthread_mutex_lock(&msgq.mx);
	while (msgq.head)
		pthread_cond_wait(&msgq.empty, &msgq.mx);
	pthread_mutex_unlock(&msgq.mx);

	pthread_mutex_lock(&client_mx);

	if (clients == c) {
		clients = clients->next;
	} else {
		for (p = clients; p && p->next != c; p = p->next)
			;
		if (p && p->next == c)
			p->next = c->next;
	}

	pthread_mutex_unlock(&client_mx);
	free(c);
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
