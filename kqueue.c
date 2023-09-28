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

#include "config.h"

#ifdef USE_KQUEUE

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "litev.h"
#include "litev-internal.h"
#include "ev_api.h"
#include "hash.h"

#define GROW	128

struct kqueue_data {
	struct hash	**hash;
	struct kevent	 *ev;
	size_t		  nev;
	size_t		  nactive_ev;
	int		  kq;
};

static short		 condition2filter(short);

static int		 kqueue_grow(struct kqueue_data *);
static EV_API_DATA	*kqueue_init(void);
static void		 kqueue_free(EV_API_DATA *);
static int		 kqueue_poll(EV_API_DATA *);
static int		 kqueue_add(EV_API_DATA *, struct litev_ev *);
static int		 kqueue_del(EV_API_DATA *, struct litev_ev *);
static int		 kqueue_close(EV_API_DATA *, int);

static short
condition2filter(short condition)
{
	switch (condition) {
	case LITEV_READ:
		return (EVFILT_READ);
	case LITEV_WRITE:
		return (EVFILT_WRITE);
	}

	assert(0);
	return (0);
}

static int
kqueue_grow(struct kqueue_data *data)
{
	struct kevent	*n_ev;
	size_t		 n_nev;

	/* No growth required. */
	if (data->nev != data->nactive_ev)
		return (LITEV_OK);

	/* Check for integer overflows. */
	if (SIZE_MAX - GROW < data->nev)
		return (LITEV_EOVERFLOW);
	n_nev = data->nev + GROW;
	if (n_nev > SIZE_MAX / sizeof(struct kevent))
		return (LITEV_EOVERFLOW);

	if ((n_ev = realloc(data->ev, sizeof(struct kevent) * n_nev)) == NULL)
		return (-1);

	data->ev = n_ev;
	data->nev = n_nev;

	return (LITEV_OK);
}

static EV_API_DATA *
kqueue_init(void)
{
	struct kqueue_data	*data;

	if ((data = malloc(sizeof(struct kqueue_data))) == NULL)
		return (NULL);

	if ((data->hash = hash_init()) == NULL)
		goto err;

	if ((data->kq = kqueue()) == -1)
		goto err;

	data->ev = NULL;
	data->nev = 0;
	data->nactive_ev = 0;

	return (data);
err:
	hash_free(&data->hash);
	free(data);
	return (NULL);
}

static void
kqueue_free(EV_API_DATA *raw_data)
{
	struct kqueue_data	*data;

	data = raw_data;

	hash_free(&data->hash);

	free(data->ev);
	close(data->kq);

	free(data);
}

static int
kqueue_poll(EV_API_DATA *raw_data)
{
	struct kqueue_data	*data;
	struct litev_ev		*ev;
	int			 nready, i;

	data = raw_data;

	nready = kevent(data->kq, NULL, 0, data->ev, data->nactive_ev, NULL);
	if (nready == -1 && errno != EINTR)
		return (-1);

	for (i = 0; i < nready; ++i) {
		/*
		 * Because kqueue(2)s udata field is set to the
		 * struct litev_ev of the appropriate hash table entry, we do
		 * not need to perform an additional lookup inside the hash
		 * table.
		 */
		ev = data->ev[i].udata;
		ev->cb(ev->fd, ev->condition, ev->udata);
	}

	return (LITEV_OK);
}

static int
kqueue_add(EV_API_DATA *raw_data, struct litev_ev *ev)
{
	struct kqueue_data	*data;
	struct hash		*node;
	struct kevent		 kev;
	short			 filter;
	int			 rc;

	data = raw_data;

	/* Check if the event is already registered. */
	if (hash_lookup(data->hash, ev) != NULL)
		return (LITEV_EEXIST);

	/* Grow data->ev, if required. */
	if ((rc = kqueue_grow(data)) != LITEV_OK)
		return (rc);

	/* Add the event to the hash table. */
	if ((rc = hash_add(data->hash, ev)) != LITEV_OK)
		return (rc);

	/* Obtain the now added node, so we can add it to kqueue(2)s udata. */
	node = hash_lookup(data->hash, ev);
	assert(node != NULL);

	/* Convert the event to a kqueue(2) event. */
	filter = condition2filter(ev->condition);
	EV_SET(&kev, ev->fd, filter, EV_ADD, 0, 0, &node->ev);

	/* Add the event to kqueue(2). */
	if (kevent(data->kq, &kev, 1, NULL, 0, NULL) == -1) {
		/* Failure during event registration. */
		hash_del(data->hash, node);
		return (-1);
	}
	++data->nactive_ev;

	return (LITEV_OK);
}

static int
kqueue_del(EV_API_DATA *raw_data, struct litev_ev *ev)
{
	struct kqueue_data	*data;
	struct hash		*node;
	struct kevent		 kev;
	short			 filter;

	data = raw_data;

	/* Check if the event is even registered. */
	if ((node = hash_lookup(data->hash, ev)) == NULL)
		return (LITEV_ENOENT);

	/* Convert the event to a kqueue(2) removal event. */
	filter = condition2filter(ev->condition);
	EV_SET(&kev, ev->fd, filter, EV_DELETE, 0, 0, NULL);

	/* Remove the event from kqueue(2). */
	if (kevent(data->kq, &kev, 1, NULL, 0, NULL) == -1)
		return (-1);

	hash_del(data->hash, node);
	--data->nactive_ev;

	return (LITEV_OK);
}

static int
kqueue_close(EV_API_DATA *raw_data, int fd)
{
	struct kqueue_data	*data;
	struct litev_ev		 ev;

	data = raw_data;
	ev.fd = fd;

	/* Remove all events that contain FD. */
	/*
	 * We do not check for the result of kqueue_del() because it may either
	 * be LITEV_ENONET, in which case it does not matter anyway, or -1, in
	 * which we will inevitably leak memory.
	 */
	ev.condition = LITEV_READ;
	kqueue_del(data, &ev);
	ev.condition = LITEV_WRITE;
	kqueue_del(data, &ev);

	/* Closing a fd removes all registered events from kqueue(2). */
	return (close(fd) == 0 ? LITEV_OK : -1);
}

void
ev_api_kqueue(struct litev_ev_api *ev_api)
{
	ev_api->init = kqueue_init;
	ev_api->free = kqueue_free;
	ev_api->poll = kqueue_poll;
	ev_api->add = kqueue_add;
	ev_api->del = kqueue_del;
	ev_api->close = kqueue_close;
}

#else

int	kqueue_dummy;

#endif
