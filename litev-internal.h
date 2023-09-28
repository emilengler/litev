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

#ifndef LITEV_INTERNAL_H
#define LITEV_INTERNAL_H

/* Opaque pointer that holds the data for a kernel event notification API. */
typedef void EV_API_DATA;

/* Structure to define the backend of a kernel event notification API. */
struct litev_ev_api {
	EV_API_DATA	*(*init)(void);
	void		 (*free)(EV_API_DATA *);

	int		 (*poll)(EV_API_DATA *);

	int		 (*add)(EV_API_DATA *, struct litev_ev *);
	int		 (*del)(EV_API_DATA *, struct litev_ev *);
	int		 (*close)(EV_API_DATA *, int);
};

struct litev_base {
	EV_API_DATA		*ev_api_data;
	struct litev_ev_api	 ev_api;

	int			 is_dispatched;
	int			 is_quitting;
};

#endif
