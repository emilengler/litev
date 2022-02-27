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
 * Minimal sample of a typical litev application.
 */

#include <sys/types.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>

#include <litev.h>

static void	sighdlr(int);

static struct litev_base	*base;

static void
sighdlr(int sig)
{
	switch (sig) {
	case SIGINT:	/* FALLTHROUGH */
	case SIGTERM:
		if (litev_break(base) != LITEV_OK)
			errx(1, "litev_break");
	}
}

int
main(int argc, char *argv[])
{
	/* Register functions to terminate the event loop. */
	signal(SIGINT, sighdlr);
	signal(SIGTERM, sighdlr);

	/* The core data structure of the event loop. */
	if ((base = litev_init()) == NULL)
		errx(1, "litev_init");

	/* Add/Remove events with litev_add(), litev_del() or litev_close(). */
	/* ... */

	/*
	 * Dispatch (aka execute) the event loop.
	 * Will run until terminated by litev_break().
	 */
	puts("Event loop dispatched");
	if (litev_dispatch(base) != LITEV_OK)
		errx(1, "litev_dispatch");
	puts("Shutting down...");

	/* Clean everything up. */
	litev_free(&base);

	return (0);
}
