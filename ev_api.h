#ifndef EV_API_H
#define EV_API_H

#ifdef USE_KQUEUE
void	ev_api_kqueue(struct litev_ev_api *);
#endif

#endif
