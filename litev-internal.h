#ifndef LITEV_INTERNAL_H
#define LITEV_INTERNAL_H

/* Detect the kernel notification API to be used. */
#if defined(_FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define USE_KQUEUE
#else
#error "only kqueue(2) is supported at the moment."
#endif

/* Opaque pointer that holds the data for a kernel notification API. */
typedef void EV_API_DATA;

/* Internal error codes. */
enum litev_err {
	LITEV_OK = 0,
	LITEV_ENOENT,
	LITEV_EBUSY,
	LITEV_EINVAL,
	LITEV_EAGAIN,
	LITEV_EALREADY,
	LITEV_EOVERFLOW
};

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
