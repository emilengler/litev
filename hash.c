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

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "litev.h"
#include "litev-internal.h"
#include "hash.h"

/* Amount of slots in the hash table array. */
#define NHASH	128

/* The hashing function. */
#define HASH(x)	(x % NHASH)

struct hash **
hash_init(void)
{
	struct hash	**ht;

	/* Allocate the hash table array and initialize it to NULL. */
	if ((ht = malloc(sizeof(struct hash *) * NHASH)) == NULL)
		return (NULL);
	memset(ht, 0, sizeof(struct hash *) * NHASH);

	return (ht);
}

void
hash_free(struct hash ***ht_ptr)
{
	struct hash	**ht, *tmp;
	int		  i;

	ht = *ht_ptr;

	/* Free all linked lists. */
	for (i = 0; i < NHASH; ++i) {
		while (ht[i] != NULL) {
			tmp = ht[i];
			ht[i] = ht[i]->next;
			free(tmp);
		}
	}

	free(ht);
	*ht_ptr = NULL;
}

struct hash *
hash_lookup(struct hash *ht[], struct litev_ev *ev)
{
	struct hash	*node;

	for (node = ht[HASH(ev->fd)]; node != NULL; node = node->next) {
		/*
		 * Nodes are identified by the unique combination of the FD
		 * and the condition.
		 */
		if (node->ev.fd == ev->fd &&
		    node->ev.condition == ev->condition)
			break;
	}

	return (node);
}

int
hash_add(struct hash *ht[], struct litev_ev *ev)
{
	struct hash	*node;
	int		 slot;

	slot = HASH(ev->fd);

	/* Allocate a new node and copy ev into it. */
	if ((node = malloc(sizeof(struct hash))) == NULL)
		return (-1);
	memcpy(&node->ev, ev, sizeof(struct litev_ev));

	/* Insert the new node at the beginning of the linked list. */
	node->prev = NULL;
	node->next = ht[slot];
	if (node->next != NULL)
		node->next->prev = node;
	ht[slot] = node;

	return (LITEV_OK);
}

void
hash_del(struct hash *ht[], struct hash *node)
{
	int	slot;

	slot = HASH(node->ev.fd);

	/* Remove node from the linked list. */
	/* node is the head node. */
	if (node->prev == NULL) {
		if (node->next != NULL)
			node->next->prev = NULL;
		ht[slot] = node->next;
	} else {
		if (node->next != NULL)
			node->next->prev = node->prev;
		node->prev->next = node->next;
	}
	free(node);
}
