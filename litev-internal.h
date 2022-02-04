#ifndef LITEV_INTERNAL_H
#define LITEV_INTERNAL_H

/* Opaque pointer that holds the data for a kernel notification API. */
typedef void EV_API_DATA;

/* Structure to define the backend of a kernel event notification API. */
struct litev_ev_api {
	EV_API_DATA	*(*init)(void);
	void		 (*free)(EV_API_DATA *);

	int		 (*poll)(EV_API_DATA *);

	int		 (*add)(EV_API_DATA *, struct litev_ev *);
	int		 (*del)(EV_API_DATA *, struct litev_ev *);
};

struct litev_base {
	EV_API_DATA		*ev_api_data;
	struct litev_ev_api	 ev_api;

	int			 is_dispatched;
	int			 is_quitting;
};

#endif
