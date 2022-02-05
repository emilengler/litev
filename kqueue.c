#include <sys/types.h>

#include <stdlib.h>

#include "litev.h"
#include "litev-internal.h"
#include "ev_api.h"

void
ev_api_kqueue(struct litev_ev_api *ev_api)
{
	ev_api->init = NULL;
	ev_api->free = NULL;
	ev_api->poll = NULL;
	ev_api->add = NULL;
	ev_api->del = NULL;
}
