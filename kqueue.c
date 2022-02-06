#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "litev.h"
#include "litev-internal.h"
#include "ev_api.h"

/*
 * When an event gets added, litev copies the entire struct litev_ev onto the
 * heap so that the caller process does not need to keep its data structure
 * around.  However, we cannot simply set kqueue(2)s udata field to our heap
 * copy of struct litev_ev because kqueue(2) offers no way of obtaining all
 * registered events, making it impossible to clean everything up.
 * To get around this issue, we will store all of our own struct litev_ev
 * copies into a doubly linked list through which we will iterate on the
 * clean-up in order to avoid memory leaks.  Because of this, kqueue(2)s udata
 * will contain a pointer to a node of this linked list rather than the plain
 * copy, making it possible to also remove nodes.
 */
struct udata_node {
	struct udata_node	*prev;
	struct udata_node	*next;
	struct litev_ev		 ev;
};

struct kqueue_data {
	struct udata_node	*head;
	struct kevent		*ev;
	size_t			 nev;
	size_t			 nactive_ev;
	int			 kq;
};

static int		 udata_add(struct udata_node **, struct litev_ev *);
static void		 udata_del(struct udata_node **, struct udata_node *);
static void		 udata_free(struct udata_node *);

static short		 kqueue_filter(short);
static int		 kqueue_grow(struct kqueue_data *);
static EV_API_DATA	*kqueue_init(void);
static void		 kqueue_free(EV_API_DATA *);
static int		 kqueue_poll(EV_API_DATA *);
static int		 kqueue_add(EV_API_DATA *, struct litev_ev *);
static int		 kqueue_del(EV_API_DATA *, struct litev_ev *);

/*
 * Copy ev into a struct udata_node and make this the new head of the
 * linked list.
 */
static int
udata_add(struct udata_node **head, struct litev_ev *ev)
{
	struct udata_node	*node;

	if ((node = malloc(sizeof(struct udata_node))) == NULL)
		return (-1);

	node->prev = NULL;
	node->next = *head;
	memcpy(&node->ev, ev, sizeof(struct litev_ev));

	/* Make node thew new head. */
	if (*head != NULL)
		(*head)->prev = node;
	*head = node;

	return (LITEV_OK);
}

/*
 * Delete node from the linked list.
 */
static void
udata_del(struct udata_node **head, struct udata_node *node)
{
	/* Removing the head node requires special care. */
	if (*head == node)
		*head = node->next;

	/* Modify the previous and next node to point to each other. */
	if (node->next != NULL)
		node->next->prev = node->prev;
	if (node->prev != NULL)
		node->prev->next = node->next;

	free(node);
}

/*
 * Delete the entire linked list.
 */
static void
udata_free(struct udata_node *head)
{
	struct udata_node	*tmp;

	while (head != NULL) {
		tmp = head;
		head = head->next;
		free(tmp);
	}
}

/*
 * Convert a litev condition to a kqueue(2) filter.
 */
static short
kqueue_filter(short condition)
{
	switch (condition) {
	case LITEV_READ:
		return (EVFILT_READ);
	case LITEV_WRITE:
		return (LITEV_WRITE);
	}

	assert(0);
	return (0);
}

/*
 * Allocate more items inside ev, once nactive_ev == nev.
 */
static int
kqueue_grow(struct kqueue_data *data)
{
	struct kevent	*n_ev;
	size_t		 n_nev;

	if (data->nactive_ev != data->nev)
		return (LITEV_OK);

	/* Check for integer overflow. */
	if (data->nev > SIZE_MAX / 128)
		return (LITEV_EOVERFLOW);
	n_nev = data->nev + 128;

	/* Check for another integer overflow for malloc(2). */
	if (n_nev > SIZE_MAX / sizeof(struct kevent))
		return (LITEV_EOVERFLOW);

	/* Allocate. */
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

	data->head = NULL;
	data->ev = NULL;
	data->nev = 0;
	data->nactive_ev = 0;

	if ((data->kq = kqueue()) == -1) {
		free(data);
		return (NULL);
	}

	return (data);
}

static void
kqueue_free(EV_API_DATA *raw_data)
{
	struct kqueue_data	*data;

	data = raw_data;
	udata_free(data->head);
	close(data->kq);
	free(data);
}

static int
kqueue_poll(EV_API_DATA *raw_data)
{
	struct kqueue_data	*data;
	struct udata_node	*node;
	int			 nready, i;

	data = raw_data;

	nready = kevent(data->kq, NULL, 0, data->ev, data->nev, NULL);
	if (nready == -1 && errno != EINTR)
		return (-1);

	for (i = 0; i < nready; ++i) {
		node = data->ev[i].udata;
		node->ev.cb(node->ev.fd, node->ev.condition, node->ev.udata);
	}

	return (LITEV_OK);
}

static int
kqueue_add(EV_API_DATA *raw_data, struct litev_ev *ev)
{
	struct kqueue_data	*data;
	struct kevent		 n_ev;
	int			 rc;
	short			 filter;

	data = raw_data;

	/* Make data->ev bigger, if required. */
	if ((rc = kqueue_grow(data)) != LITEV_OK)
		return (rc);

	/* Prepare the registration to kqueue(2). */
	if ((rc = udata_add(&data->head, ev)) != LITEV_OK)
		return (rc);
	filter = kqueue_filter(ev->condition);
	EV_SET(&n_ev, ev->fd, filter, EV_ADD, 0, 0, data->head);

	/* Register the event. */
	if (kevent(data->kq, &n_ev, 1, NULL, 0, NULL) == -1) {
		udata_del(&data->head, data->head);
		return (-1);
	}
	++data->nactive_ev;

	return (LITEV_OK);
}

static int
kqueue_del(EV_API_DATA *raw_data, struct litev_ev *ev)
{
	struct kqueue_data	*data;
	struct kevent		 d_ev;
	short			 filter;

	data = raw_data;

	/* Prepare the removal from kqueue(2). */
	filter = kqueue_filter(ev->condition);
	EV_SET(&d_ev, ev->fd, filter, EV_DELETE, 0, 0, NULL);

	/* Remove the event. */
	if (kevent(data->kq, &d_ev, 1, NULL, 0, NULL) == -1) {
		if (errno == ENOENT)
			return (LITEV_ENOENT);
		else
			return (-1);
	}
	--data->nactive_ev;

	/* TODO: Delete the node from the linked list. */

	return (LITEV_OK);
}

void
ev_api_kqueue(struct litev_ev_api *ev_api)
{
	ev_api->init = kqueue_init;
	ev_api->free = kqueue_free;
	ev_api->poll = kqueue_poll;
	ev_api->add = kqueue_add;
	ev_api->del = kqueue_del;
}
