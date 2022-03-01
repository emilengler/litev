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

#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <litev.h>

#include "perf.h"

static void	accept_cb(int, short, void *);
static void	client_cb(int, short, void *);

static struct litev_base	*base;

static void
accept_cb(int s, short condition, void *udata)
{
	struct litev_ev	ev;
	int		c;

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
		err(1, "litev_add accept_cb");
}

static void
client_cb(int c, short condition, void *udata)
{
	send(c, PERF_REPLY, strlen(PERF_REPLY), 0);
	litev_close(base, c);
}

int
main(int argc, char *argv[])
{
	struct litev_ev	ev;
	int		s;

	s = perf_socket();

	if ((base = litev_init()) == NULL)
		err(1, "litev_init");

	ev.fd = s;
	ev.condition = LITEV_READ;
	ev.cb = accept_cb;
	ev.udata = NULL;
	if (litev_add(base, &ev) != LITEV_OK)
		err(1, "litev_add");

	if (litev_dispatch(base) != LITEV_OK)
		err(1, "litev_dispatch");

	return (0);
}
