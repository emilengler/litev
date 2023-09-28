/*
 * Copyright (c) 2022 Emil Engler <me@emilengler.com>
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

#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <event.h>

#include "perf.h"

static void	accept_cb(int, short, void *);
static void	client_cb(int, short, void *);

static void
accept_cb(int s, short condition, void *udata)
{
	struct event	*ev;
	int		 c;

	/* Accept the incoming connection. */
	if ((c = accept(s, NULL, NULL)) == -1) {
		if (errno == EAGAIN)
			return;
		else
			err(1, "accept");
	}

	/* Add the new connection to the event loop. */
	if ((ev = malloc(sizeof(struct event))) == NULL)
		err(1, "malloc");
	event_set(ev, c, EV_READ, client_cb, ev);
	if (event_add(ev, NULL) != 0)
		err(1, "event_add accept_cb");
}

static void
client_cb(int c, short condition, void *ev)
{
	send(c, PERF_REPLY, strlen(PERF_REPLY), 0);
	close(c);
	free(ev);
}

int
main(int argc, char *argv[])
{
	struct event	ev;
	int		s;

	s = perf_socket();

	if (event_init() == NULL)
		err(1, "event_init");

	event_set(&ev, s, EV_READ | EV_PERSIST, accept_cb, NULL);
	if (event_add(&ev, NULL) != 0)
		err(1, "event_add main");

	if (event_dispatch() != 0)
		err(1, "event_dispatch");

	return (0);
}
