/*
 * Copyright (c) 2022 Emil Engler <engler+litev@unveil2.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Simple TCP echo server using non-blocking sockets and litev(3) for handling
 * concurrent connections.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <litev.h>

#define PORT	8080

static void	accept_cb(int, short, void *);
static void	client_cb(int, short, void *);
static int	create_socket(void);
static void	sighdlr(int);

static struct litev_base	*base;

/*
 * The callback for incoming connections.
 */
static void
accept_cb(int s, short condition, void *unused)
{
	struct litev_ev	ev;
	int		c;

	assert(condition == LITEV_READ);

	/* Accept the incoming connection. */
	if ((c = accept(s, NULL, NULL)) == -1) {
		if (errno == EAGAIN)
			return;
		else
			err(1, "accept");
	}

	/* Add the new connection to the event loop. */
	ev.fd = c;
	ev.condition = LITEV_READ;
	ev.cb = client_cb;
	ev.udata = NULL;
	if (litev_add(base, &ev) != LITEV_OK)
		errx(1, "litev_add client");
}

/*
 * The callback for already connected clients.
 */
static void
client_cb(int c, short condition, void *unused)
{
	unsigned char	buf[129];
	struct litev_ev	ev;
	ssize_t		n;

	assert(condition == LITEV_READ);

	memset(buf, '\0', sizeof(buf));

	n = recv(c, buf, 128, 0);
	if (n == -1 && errno != EAGAIN)
		err(1, "recv");
	else if (n == 0) {	/* Client closed the connection. */
		litev_close(base, c);
	} else
		send(c, buf, 129, 0);
}

static int
create_socket(void)
{
	struct sockaddr_in	sa;
	int			s, flags;

	/* Create the socket. */
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	/* Set the socket non-blocking. */
	if ((flags = fcntl(s, F_GETFL)) == -1)
		err(1, "fcntl F_GETFL");
	if (fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1)
		err(1, "fcntl F_SETFL");

	/* Bind the socket to port 8080. */
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(PORT);
	if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) == -1)
		err(1, "bind");

	/* Indicate that we are a server. */
	if (listen(s, 128) == -1)
		err(1, "listen");

	return (s);
}

static void
sighdlr(int sig)
{
	switch (sig) {
	case SIGINT:	/* FALLTHROUGH */
	case SIGTERM:
		litev_break(base);
		break;
	}
}

int
main(int argc, char *argv[])
{
	struct litev_ev	ev;
	int		s;

	signal(SIGINT, sighdlr);
	signal(SIGTERM, sighdlr);

	/* Create the base for the event loop. */
	if ((base = litev_init()) == NULL)
		errx(1, "litev_init");

	/* Create the server socket. */
	s = create_socket();

	/* Add the server socket to the event loop. */
	ev.fd = s;
	ev.condition = LITEV_READ;
	ev.cb = accept_cb;
	ev.udata = NULL;
	if (litev_add(base, &ev) != LITEV_OK)
		errx(1, "litev_add master");

	puts("Ready for incoming connections");
	/* Launch the event loop. */
	if (litev_dispatch(base) != LITEV_OK)
		errx(1, "litev_dispatch");
	puts("Shutting down...");

	/* Clean everything up. */
	litev_free(&base);
	close(s);

	return (0);
}
