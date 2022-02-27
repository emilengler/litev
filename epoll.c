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

/* Unfortunately, we cannot name it epoll_data. */
struct epoll_api_data {
	struct hash		**hash;
	struct epoll_event	 *ev;
	size_t			  nev;
	size_t			  nactive_ev;
	int			  epfd;
};

static uint32_t		 condition2event(short);

static void		 epoll_cb(struct hash *[], int, short);
static int		 epoll_grow(struct epoll_api_data *);

static EV_API_DATA	*epoll_init(void);
static void		 epoll_free(EV_API_DATA *);
static int		 epoll_poll(EV_API_DATA *);
static int		 epoll_add(EV_API_DATA *, struct litev_ev *);
static int		 epoll_del(EV_API_DATA *, struct litev_ev *);
static int		 epoll_close(EV_API_DATA *, int);

/*
 * TODO: Distinguish the name from condition2event() in poll.c.
 */
static uint32_t
condition2event(short condition)
{
	switch (condition) {
	case LITEV_READ:
		return (EPOLLIN);
	case LITEV_WRITE:
		return (EPOLLOUT);
	}

	assert(0);
	return (0);
}

/*
 * Lookup an event from the hash table identified by the FD and the condition
 * and execute the accompanying callback function.  See the comment inside
 * epoll_poll() for the motivation behind this approach.
 */
static void
epoll_cb(struct hash *ht[], int fd, short condition)
{
	struct hash	*node;
	struct litev_ev	 ev;

	ev.fd = fd;
	ev.condition = condition;

	/* Lookup the event and do nothing, if it could not be found. */
	if ((node = hash_lookup(ht, &ev)) == NULL)
		return;

	/* Finally execute the callback. */
	node->ev.cb(fd, condition, node->ev.udata);
}

static int
epoll_grow(struct epoll_api_data *data)
{
	struct epoll_event	*n_ev;
	size_t			 n_nev;

	/* No growth required. */
	if (data->nev != data->nactive_ev)
		return (LITEV_OK);

	/* Check for integer overflows. */
	if (SIZE_MAX - GROW < data->nev)
		return (LITEV_EOVERFLOW);
	n_nev = data->nev + GROW;
	if (n_nev > SIZE_MAX / sizeof(struct epoll_event))
		return (LITEV_EOVERFLOW);

	n_ev = realloc(data->ev, sizeof(struct epoll_event) * n_nev);
	if (n_ev == NULL)
		return (-1);

	data->ev = n_ev;
	data->nev = n_nev;

	return (LITEV_OK);
}

static EV_API_DATA *
epoll_init(void)
{
	struct epoll_api_data	*data;

	if ((data = malloc(sizeof(struct epoll_api_data))) == NULL)
		return (NULL);

	if ((data->hash = hash_init()) == NULL)
		goto err;

	if ((data->epfd = epoll_create(1)) == -1)
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
epoll_free(EV_API_DATA *raw_data)
{
	struct epoll_api_data	*data;

	data = raw_data;

	hash_free(&data->hash);

	free(data->ev);
	close(data->epfd);

	free(data);
}

static int
epoll_poll(EV_API_DATA *raw_data)
{
	struct epoll_api_data	*data;
	int			 nready, i;

	data = raw_data;

	/* data is not yet "ready" for epoll_wait(2). */
	if (data->nactive_ev == 0 || data->ev == NULL)
		return (LITEV_OK);

	nready = epoll_wait(data->epfd, data->ev, data->nactive_ev, -1);
	if (nready == -1 && errno != EINTR)
		return (-1);

	for (i = 0; i < nready; ++i) {
		/*
		 * Because epoll(2) works on a per-FD basis, rather than on a
		 * per-event basis, we cannot make use of the opaque pointer
		 * field inside union epoll_data to store our event.
		 * Instead, we check the available conditions for the FD and
		 * lookup the accompanying event inside the hash table.
		 * All of this is being done by the epoll_cb() function.
		 */
		if (data->ev[i].events & EPOLLIN)
			epoll_cb(data->hash, data->ev[i].data.fd, LITEV_READ);
		if (data->ev[i].events & EPOLLOUT)
			epoll_cb(data->hash, data->ev[i].data.fd, LITEV_WRITE);
	}

	return (LITEV_OK);
}

static int
epoll_add(EV_API_DATA *raw_data, struct litev_ev *ev)
{
	struct epoll_api_data	*data;
	struct hash		*node;
	struct epoll_event	 eev;
	int			 rc;

	data = raw_data;

	/* Check if the event is already registered. */
	if (hash_lookup(data->hash, ev) != NULL)
		return (LITEV_EEXIST);

	/*
	 * Grow data->ev, if required.  This step must be done, before
	 * nactive_ev is being incremented!
	 */
	if ((rc = epoll_grow(data)) != LITEV_OK)
		return (rc);

	/*
	 * Because epoll(2) works on a per-FD basis, rather than on a
	 * per-event basis, we have to use a very dirty hack here in
	 * order to register new events.
	 * It works as follows: Suppose that the application has already
	 * registered a FD with LITEV_READ and now wants to register the
	 * same FD with the LITEV_WRITE condition.  What litev will do,
	 * is try to add the FD with EPOLLOUT through EPOLL_CTL_ADD.
	 * If this fails with EEXIST, litev will use EPOLL_CTL_MOD and
	 * OR the event with LITEV_READ.
	 * This approach makes it hard to add more conditions and should
	 * be replaced by a better solution, once found.
	 */
	eev.events = condition2event(ev->condition);
	eev.data.fd = ev->fd;
	if (epoll_ctl(data->epfd, EPOLL_CTL_ADD, ev->fd, &eev) == -1) {
		if (errno != EEXIST)
			return (-1);

		eev.events = EPOLLIN | EPOLLOUT;
		if (epoll_ctl(data->epfd, EPOLL_CTL_MOD, ev->fd, &eev) == -1)
			return (-1);
	}
	++data->nactive_ev;

	/* Add the event to the hash table. */
	if ((rc = hash_add(data->hash, ev)) != LITEV_OK)
		goto err;

	return (LITEV_OK);
err:
	epoll_ctl(data->epfd, EPOLL_CTL_DEL, ev->fd, &eev);
	--data->nactive_ev;
	node = hash_lookup(data->hash, ev);
	if (node != NULL)
		hash_del(data->hash, node);
	return (rc);
}

static int
epoll_del(EV_API_DATA *raw_data, struct litev_ev *ev)
{
	return (LITEV_OK);
}

static int
epoll_close(EV_API_DATA *raw_data, int fd)
{
	struct epoll_api_data	*data;
	struct hash		*ev_read, *ev_write;
	struct litev_ev		 ev;

	data = raw_data;
	ev.fd = fd;

	/* Fetch all events that contain fd. */
	ev.condition = LITEV_READ;
	if ((ev_read = hash_lookup(data->hash, &ev)) != NULL)
		hash_del(data->hash, ev_read);
	ev.condition = LITEV_WRITE;
	if ((ev_write = hash_lookup(data->hash, &ev)) != NULL)
		hash_del(data->hash, ev_write);

	/* Closing a fd removes all registered events from epoll(2). */
	return (close(fd) == 0 ? LITEV_OK : -1);
}

void
ev_api_epoll(struct litev_ev_api *ev_api)
{
	ev_api->init = epoll_init;
	ev_api->free = epoll_free;
	ev_api->poll = epoll_poll;
	ev_api->add = epoll_add;
	ev_api->del = epoll_del;
	ev_api->close = epoll_close;
}

#else

int	epoll_dummy;

#endif
