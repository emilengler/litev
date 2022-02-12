/*
 * Copyright (c) 2022 Emil Engler <engler+litev@unveil2.org>
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

#ifndef LITEV_H
#define LITEV_H

#ifdef __cplusplus
extern "C" {
#endif

#define LITEV_READ	1
#define LITEV_WRITE	2

struct litev_base;
struct litev_ev;

enum {
	LITEV_OK = 0,
	LITEV_ENOENT,
	LITEV_EBUSY,
	LITEV_EEXISTS,
	LITEV_EINVAL,
	LITEV_EAGAIN,
	LITEV_EALREADY,
	LITEV_EOVERFLOW
};

struct litev_ev {
	int	  fd;
	short	  condition;
	void	(*cb)(int, short, void *);
	void	 *udata;
};

struct litev_base	*litev_init(void);
void			 litev_free(struct litev_base **);

int			 litev_dispatch(struct litev_base *);
int			 litev_break(struct litev_base *);

int			 litev_add(struct litev_base *, struct litev_ev *);
int			 litev_del(struct litev_base *, struct litev_ev *);
int			 litev_close(struct litev_base *, int);

#ifdef __cplusplus
}
#endif

#endif
