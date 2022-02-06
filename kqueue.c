#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

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

int	udata_add(struct udata_node **, struct litev_ev *);
void	udata_del(struct udata_node **, struct udata_node *);
void	udata_free(struct udata_node *);

/*
 * Copy ev into a struct udata_node and make this the new head of the
 * linked list.
 */
int
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
void
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
void
udata_free(struct udata_node *head)
{
	struct udata_node	*tmp;

	while (head != NULL) {
		tmp = head;
		head = head->next;
		free(tmp);
	}
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
