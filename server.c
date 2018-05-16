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

#define  PROGNAME "server"
#include "util.h"

struct client {
	int fd;
	FILE *fp;
	char name[NAMESIZE];
	struct client *next;
};

/* each message contains the function
 * that  will dispatch the message */
struct msg {
	void (*send)(struct msg*);
	char buf[BUFSIZE];
	const struct client *sender;
	struct msg *next;
};

typedef void (*dispatch)(struct msg*);

/* client linked list header and mutex */
struct client *clients;
pthread_mutex_t client_mx = PTHREAD_MUTEX_INITIALIZER;

/* client api */
struct client *client_add(int, const char *);
void client_rm(struct client *);

/* msg queue api */
struct msg *msg_push(dispatch type, const struct client *, const char *);
struct msg *msg_pop(void);

/* msg dispatchers */
void msg_enter(struct msg*);
void msg_relay(struct msg*);
void msg_left(struct msg*);

/* global msg queue object */
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

int tcpbind(const char *port);

/* thread entrypoints */
void *serve(void *);
void *broadcast(void *);

int main(int argc, char **argv)
{
	int sfd, cfd;
	int *arg;
	char *port = DEFPORT;
	pthread_t tid;
	struct sockaddr_in peer_addr;
	socklen_t peer_sz = sizeof(peer_addr);

	if (argc > 2)
		die("usage: %s [port]", argv[0]);

	if (argc > 1)
		port = argv[1];

	setbuf(stdout, NULL);

	/* We need to ignore SIGPIPE
	 *
	 * Potentially the broadcast thread may
	 * be writing to a client that has just momentarily
	 * left. We can just ignore it completely, without
	 * any harm.
	 */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		die("ignore SIGPIPE:");

	sfd = tcpbind(port);
	printf("[%s] listening on port %s\n", PROGNAME, port);

	/* spawn and detach the thread to send messages */
	if ((errno = pthread_create(&tid, NULL, broadcast, NULL)))
		die("spawning broadcast thread:");
	if ((errno = pthread_detach(tid)))
		die("detaching broadcast thread:");

	// continuously accept clients
	while ((cfd = accept(sfd, (struct sockaddr *)&peer_addr, &peer_sz)) != -1) {
		/* alloc mem for the arg, since we will reuse
		 * cfd in this function.
		 * The thread will free it. */
		arg = dmalloc(sizeof(int));
		*arg = cfd;

		/* create and detach */
		if ((errno = pthread_create(&tid, NULL, serve, arg)))
			die("spawning client thread:");
		if ((errno = pthread_detach(tid)))
			die("detaching client thread:");
	}

	/* error if here */
	die("accept():");

	/* unreachable */
	close(sfd);
}

/* Per client thread */
void *serve(void *pfd)
{
	int fd;
	char name[NAMESIZE];
	char buf[BUFSIZE];
	struct client *c;

	if (!pfd || (fd = *(int *)pfd) <= 0)
		return NULL;

	/* pointer was malloc'd by main thread */
	free(pfd);

	/* get client name */
	if (read_line(fd, name, NAMESIZE) < 1)
		return NULL;

	c = client_add(fd, name);
	msg_push(msg_enter, c, NULL);

	/* read while the socket is open */
	while (read_line(fd, buf, BUFSIZE) > 0)
		msg_push(msg_relay, c, buf);

	/* send client left message */
	msg_push(msg_left, c, NULL);

	/* remove the client, close fd */
	client_rm(c);

	return NULL;
}

/* thread for sending messages */
void *broadcast(void *arg)
{
	struct msg *m;

	while ((m = msg_pop())) {
		m->send(m);
		free(m);
	}

	/* unreachable */
	return NULL;
}

/* add client to linked list */
struct client *client_add(int fd, const char *name)
{
	struct client *c = dmalloc(sizeof(struct client));
	c->fd = fd;
	strcpy(c->name, name);

	/* wrap the socket in a file pointer */
	if (!(c->fp = fdopen(c->fd, "w")))
		die("fdopen():");

	/* setup line buffering, since all
	 * messages are newline delimited
	 */
	setlinebuf(c->fp);

	pthread_mutex_lock(&client_mx);

	if (clients)
		c->next = clients;
	else
		c->next = NULL;

	clients = c;

	pthread_mutex_unlock(&client_mx);

	return c;
}

/* Removes the client from the linked list.
 * Ensures that this clients messages are
 * all sent.
 */
void client_rm(struct client *c)
{
	struct client *p;

	/* to ensure that all of this clients messages
	 * have been sent we simply wait until the
	 * entire msg queue is empty.
	 */

	/* block while queue is not empty */
	pthread_mutex_lock(&msgq.mx);
	while (msgq.head)
		pthread_cond_wait(&msgq.empty, &msgq.mx);
	pthread_mutex_unlock(&msgq.mx);


	/* traverse the client list
	 * could be more efficient with doubly linked list
	 */
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

	/* closes the file descriptor as well */
	fclose(c->fp);
	free(c);
}


/* dispatcher for announcing new client
 * The new client is greeted, while everyone
 * else is informed of the new client */
void msg_enter(struct msg *m)
{
	struct client *c;
	int first = 1;

	/* Writing to client may cause an EPIPE
	 * since the client may have already quit.
	 * This is safely ignored.
	 *
	 * No checks are done, its just ignored.
	 */

	fprintf(m->sender->fp, PROGNAME "\nWelcome %s!\n", m->sender->name);
	fprintf(m->sender->fp, PROGNAME "\n");

	pthread_mutex_lock(&client_mx);

	for (c = clients; c; c = c->next) {
		if (c == m->sender)
			continue;

		if (first) {
			first = 0;
			fprintf(m->sender->fp, "In chatroom: ");
		}

		/* ignore all write errors */
		fprintf(m->sender->fp, "%s", c->name);
		if (c->next)
			fprintf(m->sender->fp, ", ");

		fprintf(c->fp, PROGNAME "\n%s has entered\n", m->sender->name);
	}

	pthread_mutex_unlock(&client_mx);

	if (first)
		fprintf(m->sender->fp, "Chatroom empty");

	fprintf(m->sender->fp, "\n");

	printf("[%s] %s has entered\n", PROGNAME, m->sender->name);
}

/* send a standard message
 * Everyone but the sender is
 * sent the message */
void msg_relay(struct msg *m)
{
	struct client *c;

	pthread_mutex_lock(&client_mx);

	for (c = clients; c; c = c->next) {
		if (c == m->sender)
			continue;

		fprintf(c->fp, "%s\n%s\n", m->sender->name, m->buf);
	}

	pthread_mutex_unlock(&client_mx);

	printf("[%s] %s\n", m->sender->name, m->buf);
}

/* announce that the client has left */
void msg_left(struct msg *m)
{
	struct client *c;

	pthread_mutex_lock(&client_mx);

	for (c = clients; c; c = c->next) {
		if (c == m->sender)
			continue;
		fprintf(c->fp, PROGNAME "\n%s has left\n", m->sender->name);
	}

	pthread_mutex_unlock(&client_mx);

	printf("[%s] %s has left\n", PROGNAME, m->sender->name);
}

/* push a message on the queue */
struct msg *msg_push(dispatch type, const struct client *sender, const char *buf)
{
	struct msg *m;

	m = dmalloc(sizeof(struct msg));
	m->send = type;
	m->sender = sender;
	if (buf)
		strcpy(m->buf, buf);

	pthread_mutex_lock(&msgq.mx);

	if (msgq.tail) {
		msgq.tail->next = m;
		msgq.tail = m;
	} else {
		if (msgq.head)
			die("message queue misalignment");
		msgq.head = msgq.tail = m;
	}

	m->next = NULL;

	pthread_mutex_unlock(&msgq.mx);

	/* signal that the queue has messages */
	pthread_cond_broadcast(&msgq.has);

	return m;
}

/* remove a message from the queue */
struct msg *msg_pop()
{
	struct msg *p;

	pthread_mutex_lock(&msgq.mx);

	/* wait until there is a message */
	while (!msgq.head) {
		/* signal empty queue */
		pthread_cond_broadcast(&msgq.empty);
		pthread_cond_wait(&msgq.has, &msgq.mx);
	}

	p = msgq.head;
	msgq.head = (msgq.head)->next;

	/* need to NULL the tail if the queue is empty */
	if (p == msgq.tail) {
		if (msgq.head)
			die("message queue misalignment");
		msgq.tail = NULL;
	}

	pthread_mutex_unlock(&msgq.mx);

	return p;
}

/*
 * allocate the socket and bind to port
 */
int tcpbind(const char *port)
{
	int fd;
	unsigned short nport;
	struct sockaddr_in sin;

	if (!(nport = atoport(port)))
		die("%s: not a valid port number", port);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(nport);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((fd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
		die("socket():");

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
		die("bind():");

	if (listen(fd, 50) < 0)
		die("listen():");

	return fd;
}
