/*
 * Copyright (c) 2022-2023 Emil Engler <me@emilengler.com>
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

#include <netinet/in.h>

#include <err.h>
#include <fcntl.h>
#include <string.h>

#include "perf.h"

static const int	true = 1;

int
perf_socket(void)
{
	struct sockaddr_in	sa;
	int			s, flags;

	/* Create the socket. */
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	/* Allow address reusing. */
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &true, sizeof(true)) ==
	    -1)
		err(1, "setsockopt SO_REUSEADDR");

	/* Set the socket non-blocking. */
	if ((flags = fcntl(s, F_GETFL)) == -1)
		err(1, "fcntl F_GETFL");
	if (fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1)
		err(1, "fcntl F_SETFL");

	/* Bind the socket to port 8080. */
	memset(&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(8080);
	if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) == -1)
		err(1, "bind");

	/* Indicate that we are a server. */
	if (listen(s, 128) == -1)
		err(1, "listen");

	return (s);
}
