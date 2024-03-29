/*
 * Copyright (c) 2022-2024 Emil Engler <me@emilengler.com>
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

#include <sys/types.h>

#include <stdlib.h>

#include "litev.h"
#include "litev-internal.h"
#include "ev_api.h"

struct litev_base *
litev_init(void)
{
	struct litev_base	*base;

	if ((base = malloc(sizeof(struct litev_base))) == NULL)
		return (NULL);

#if defined(USE_KQUEUE)
	ev_api_kqueue(&base->ev_api);
#elif defined(USE_EPOLL)
	ev_api_epoll(&base->ev_api);
#elif defined(USE_POLL)
	ev_api_poll(&base->ev_api);
#endif

	if ((base->ev_api_data = base->ev_api.init()) == NULL) {
		free(base);
		return (NULL);
	}

	base->is_dispatched = 0;
	base->is_quitting = 0;

	return (base);
}

void
litev_free(struct litev_base **base)
{
	if (base == NULL || *base == NULL)
		return;

	(*base)->ev_api.free((*base)->ev_api_data);
	free(*base);
	*base = NULL;
}

int
litev_dispatch(struct litev_base *base)
{
	int	rc;

	if (base == NULL)
		return (LITEV_EINVAL);

	/* The event loop cannot run concurrently. */
	if (base->is_dispatched)
		return (LITEV_EBUSY);

	base->is_dispatched = 1;
	while (!base->is_quitting) {
		if ((rc = base->ev_api.poll(base->ev_api_data)) != LITEV_OK)
			return (rc);
	}

	return (LITEV_OK);
}

int
litev_break(struct litev_base *base)
{
	if (base == NULL)
		return (LITEV_EINVAL);

	if (base->is_quitting)
		return (LITEV_EALREADY);

	/* The event loop must be dispatched in order to be stopped. */
	if (!base->is_dispatched)
		return (LITEV_EAGAIN);

	base->is_quitting = 1;

	return (LITEV_OK);
}

int
litev_add(struct litev_base *base, struct litev_ev *ev)
{
	if (base == NULL || ev == NULL || ev->fd < 0)
		return (LITEV_EINVAL);
	if (!(ev->condition == LITEV_READ || ev->condition == LITEV_WRITE))
		return (LITEV_EINVAL);

	return (base->ev_api.add(base->ev_api_data, ev));
}

int
litev_del(struct litev_base *base, struct litev_ev *ev)
{
	if (base == NULL || ev == NULL || ev->fd < 0)
		return (LITEV_EINVAL);
	if (!(ev->condition == LITEV_READ || ev->condition == LITEV_WRITE))
		return (LITEV_EINVAL);

	return (base->ev_api.del(base->ev_api_data, ev));
}

int
litev_close(struct litev_base *base, int fd)
{
	if (base == NULL || fd < 0)
		return (LITEV_EINVAL);

	return (base->ev_api.close(base->ev_api_data, fd));
}
