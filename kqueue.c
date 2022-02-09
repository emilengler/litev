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

#define GROW	128
#define NHASH	1024

#define HASH(x)	(x % NHASH)

/*
 * When an event gets added, litev copies the struct litev_ev onto the heap,
 * so that the caller process does not need to keep its data around.
 * However, this also requires us to keep track of our allocations, so we do
 * not leak memory when events get removed or the event loop is free'd
 * entirely.
 * To get around this problem, we will use a hash map with a pair of the FD
 * and the condition as its identifier.
 */
struct hash_node {
	struct hash_node	*prev;
	struct hash_node	*next;
	struct litev_ev		 ev;
};

struct kqueue_data {
	struct hash_node	**hash;
	struct kevent		 *ev;
	size_t			  nev;
	size_t			  nactive_ev;
	int			  kq;
};

static short		 condition2filter(short);

static int		 hash_add(struct hash_node *[], struct litev_ev *);
static void		 hash_del(struct hash_node *[], struct hash_node *);
static void		 hash_free(struct hash_node *[]);
static struct hash_node	*hash_lookup(struct hash_node *[], struct litev_ev *);

static int		 kqueue_grow(struct kqueue_data *);
static EV_API_DATA	*kqueue_init(void);
static void		 kqueue_free(EV_API_DATA *);
static int		 kqueue_poll(EV_API_DATA *);
static int		 kqueue_add(EV_API_DATA *, struct litev_ev *);
static int		 kqueue_del(EV_API_DATA *, struct litev_ev *);

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
hash_add(struct hash_node *hash[], struct litev_ev *ev)
{
	struct hash_node	*node;
	int			 index;

	index = HASH(ev->fd);

	if ((node = malloc(sizeof(struct hash_node))) == NULL)
		return (-1);

	memcpy(&node->ev, ev, sizeof(struct litev_ev));

	/* Insert the new node at the beginning of the linked list. */
	node->prev = NULL;
	node->next = hash[index];
	if (node->next != NULL)
		node->next->prev = node;
	hash[index] = node;

	return (LITEV_OK);
}

static void
hash_del(struct hash_node *hash[], struct hash_node *node)
{
	int index;

	index = HASH(node->ev.fd);

	/* node is the head node. */
	if (node->prev == NULL) {
		if (node->next != NULL)
			node->next->prev = NULL;
		hash[index] = node->next;
	} else {
		if (node->next != NULL)
			node->next->prev = node->prev;
		node->prev->next = node->next;
	}
	free(node);
}

static void
hash_free(struct hash_node *hash[])
{
	struct hash_node	*tmp;
	int			 i;

	for (i = 0; i < NHASH; ++i) {
		while (hash[i] != NULL) {
			tmp = hash[i];
			hash[i] = hash[i]->next;
			free(tmp);
		}
	}

	free(hash);
}

static struct hash_node *
hash_lookup(struct hash_node *hash[], struct litev_ev *ev)
{
	struct hash_node	*node;

	for (node = hash[HASH(ev->fd)]; node != NULL; node = node->next) {
		if (node->ev.fd == ev->fd &&
		    node->ev.condition == ev->condition)
			break;
	}

	return (node);
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

	if ((data->hash = malloc(sizeof(struct hash_node *) * NHASH)) == NULL)
		goto err;
	memset(data->hash, 0, sizeof(struct hash_node *) * NHASH);

	if ((data->kq = kqueue()) == -1)
		goto err;

	data->ev = NULL;
	data->nev = 0;
	data->nactive_ev = 0;

	return (data);
err:
	free(data->hash);
	free(data);
	return (NULL);
}

static void
kqueue_free(EV_API_DATA *raw_data)
{
	struct kqueue_data	*data;

	data = raw_data;

	hash_free(data->hash);

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
		 * struct litev_ev of the appropriate hash map entry, we do
		 * not need to perform an additional lookup inside the hash
		 * map.
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
	struct hash_node	*node;
	struct kevent		 kev;
	short			 filter;
	int			 rc;

	data = raw_data;

	/* Check if the event is already registered. */
	if (hash_lookup(data->hash, ev) != NULL)
		return (LITEV_EEXISTS);

	/* Grow data->ev, if required. */
	if ((rc = kqueue_grow(data)) != LITEV_OK)
		return (rc);

	/* Add the event to the hash map. */
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
	struct hash_node	*node;
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

void
ev_api_kqueue(struct litev_ev_api *ev_api)
{
	ev_api->init = kqueue_init;
	ev_api->free = kqueue_free;
	ev_api->poll = kqueue_poll;
	ev_api->add = kqueue_add;
	ev_api->del = kqueue_del;
}
