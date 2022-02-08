#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "litev.h"
#include "litev-internal.h"
#include "ev_api.h"

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

static int		 hash_add(struct hash_node *[], struct litev_ev *);
static void		 hash_del(struct hash_node *[], struct hash_node *);
static void		 hash_free(struct hash_node *[]);
static struct hash_node	*hash_lookup(struct hash_node *[], struct litev_ev *);

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

void
ev_api_kqueue(struct litev_ev_api *ev_api)
{
	ev_api->init = NULL;
	ev_api->free = NULL;
	ev_api->poll = NULL;
	ev_api->add = NULL;
	ev_api->del = NULL;
}
