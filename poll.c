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

#ifdef USE_POLL

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "litev.h"
#include "litev-internal.h"
#include "ev_api.h"

#define GROW	128

/*
 * pfd_ev shares the indices with pfd, so the look-ups for the appropriate
 * callbacks with their udata are O(1).
 *
 * TODO: Add a hash table, so that removals are O(1).
 */
struct poll_data {
	struct pollfd	*pfd;
	struct litev_ev	*pfd_ev;
	size_t		 npfd;
};

static short		 condition2event(short);

static size_t		 poll_find_ev(struct poll_data *, struct litev_ev *);
static size_t		 poll_find_free(struct poll_data *);
static int		 poll_grow(struct poll_data *);

static EV_API_DATA	*poll_init(void);
static void		 poll_free(EV_API_DATA *);
static int		 poll_poll(EV_API_DATA *);
static int		 poll_add(EV_API_DATA *, struct litev_ev *);
static int		 poll_del(EV_API_DATA *, struct litev_ev *);
static int		 poll_close(EV_API_DATA *, int);

/*
 * Convert a litev condition to an event that poll(2) understands.
 */
static short
condition2event(short condition)
{
	switch (condition) {
	case LITEV_READ:
		return (POLLIN);
	case LITEV_WRITE:
		return (POLLOUT);
	}

	assert(0);
	return (0);
}

/*
 * Return the index of the first occurrence of ev inside pfd or npfd in case
 * that the event does not exist within pfd.
 */
static size_t
poll_find_ev(struct poll_data *data, struct litev_ev *ev)
{
	size_t	i;

	for (i = 0; i < data->npfd; ++i) {
		if (data->pfd_ev[i].fd == ev->fd &&
		    data->pfd_ev[i].condition == ev->condition)
			break;
	}

	return (i);
}

/*
 * Return the index of the first free slot inside pfd or npfd in case that pfd
 * is full.
 */
static size_t
poll_find_free(struct poll_data *data)
{
	size_t	i;

	for (i = 0; i < data->npfd; ++i) {
		if (data->pfd[i].fd == -1)
			break;
	}

	return (i);
}

/*
 * Grow pfd and pfd_ev by GROW and initialize the new slots.
 */
static int
poll_grow(struct poll_data *data)
{
	struct pollfd	*n_pfd;
	struct litev_ev	*n_pfd_ev;
	size_t		 n_npfd, i;

	/* Check for integer overflows. */
	if (SIZE_MAX - GROW < data->npfd)
		return (LITEV_EOVERFLOW);
	n_npfd = data->npfd + GROW;
	if (n_npfd > SIZE_MAX / sizeof(struct pollfd))
		return (LITEV_EOVERFLOW);
	if (n_npfd > SIZE_MAX / sizeof(struct litev_ev))
		return (LITEV_EOVERFLOW);

	/* Allocate the new space. */
	n_pfd = realloc(data->pfd, sizeof(struct pollfd) * n_npfd);
	if (n_pfd == NULL)
		return (-1);
	data->pfd = n_pfd;

	n_pfd_ev = realloc(data->pfd_ev, sizeof(struct litev_ev) * n_npfd);
	if (n_pfd_ev == NULL)
		return (-1);
	data->pfd_ev = n_pfd_ev;

	/* Initialize the new slots. */
	for (i = data->npfd; i < n_npfd; ++i) {
		data->pfd[i].fd = -1;
		data->pfd[i].events = 0;
		data->pfd[i].revents = 0;
		data->pfd_ev[i].fd = -1;
		data->pfd_ev[i].condition = 0;
	}

	data->npfd = n_npfd;

	return (LITEV_OK);
}

static EV_API_DATA *
poll_init(void)
{
	struct poll_data	*data;

	if ((data = malloc(sizeof(struct poll_data))) == NULL)
		return (NULL);

	data->pfd = NULL;
	data->pfd_ev = NULL;
	data->npfd = 0;

	return (data);
}

static void
poll_free(EV_API_DATA *raw_data)
{
	struct poll_data	*data;

	data = raw_data;

	free(data->pfd);
	free(data->pfd_ev);

	free(data);
}

static int
poll_poll(EV_API_DATA *raw_data)
{
	struct poll_data	*data;
	size_t			 i;
	short			 revent;

	data = raw_data;

	if (poll(data->pfd, data->npfd, -1) == -1 && errno != EINTR)
		return (-1);

	for (i = 0; i < data->npfd; ++i) {
		/* Just to not exceed 79 characters in the if statement. :^) */
		revent = data->pfd[i].revents;
		if (revent == POLLIN || revent == POLLOUT) {
			data->pfd_ev[i].cb(data->pfd_ev[i].fd,
					   data->pfd_ev[i].condition,
					   data->pfd_ev[i].udata);
		}
	}

	return (LITEV_OK);
}

static int
poll_add(EV_API_DATA *raw_data, struct litev_ev *ev)
{
	struct poll_data	*data;
	size_t			 slot;
	int			 rc;

	data = raw_data;

	/* Check if the event is already registered. */
	if (poll_find_ev(data, ev) != data->npfd)
		return (LITEV_EEXIST);

	/* Check if we need to allocate more space for events. */
	if ((slot = poll_find_free(data)) == data->npfd) {
		if ((rc = poll_grow(data)) != LITEV_OK)
			return (rc);
	}

	data->pfd[slot].fd = ev->fd;
	data->pfd[slot].events = condition2event(ev->condition);
	data->pfd[slot].revents = 0;
	memcpy(&data->pfd_ev[slot], ev, sizeof(struct litev_ev));

	return (LITEV_OK);
}

static int
poll_del(EV_API_DATA *raw_data, struct litev_ev *ev)
{
	struct poll_data	*data;
	size_t			 slot;

	data = raw_data;

	/* Check if the event is even registered. */
	if ((slot = poll_find_ev(data, ev)) == data->npfd)
		return (LITEV_ENOENT);

	/* Remove the event from poll(2). */
	data->pfd[slot].fd = -1;
	data->pfd[slot].events = 0;
	data->pfd[slot].revents = 0;
	data->pfd_ev[slot].fd = -1;
	data->pfd_ev[slot].condition = 0;

	return (LITEV_OK);
}

static int
poll_close(EV_API_DATA *raw_data, int fd)
{
	struct poll_data	*data;
	struct litev_ev		 ev;

	data = raw_data;
	ev.fd = fd;

	/* Remove all possible events that could contain fd. */
	ev.condition = LITEV_READ;
	poll_del(data, &ev);
	ev.condition = LITEV_WRITE;
	poll_del(data, &ev);

	return (close(fd) == 0 ? LITEV_OK : -1);
}

void
ev_api_poll(struct litev_ev_api *ev_api)
{
	ev_api->init = poll_init;
	ev_api->free = poll_free;
	ev_api->poll = poll_poll;
	ev_api->add = poll_add;
	ev_api->del = poll_del;
	ev_api->close = poll_close;
}

#else

int	poll_dummy;

#endif
