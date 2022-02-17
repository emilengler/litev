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

#include "config.h"

#ifdef USE_EPOLL

#include <sys/types.h>
#include <sys/epoll.h>

#include <stdlib.h>
#include <unistd.h>

#include "litev.h"
#include "litev-internal.h"
#include "ev_api.h"

void
ev_api_epoll(struct litev_ev_api *ev_api)
{
	ev_api->init = NULL;
	ev_api->free = NULL;
	ev_api->poll = NULL;
	ev_api->add = NULL;
	ev_api->del = NULL;
	ev_api->close = NULL;
}

#else

int	epoll_dummy;

#endif
