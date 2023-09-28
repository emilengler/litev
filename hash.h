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

#ifndef HASH_H
#define HASH_H

/*
 * TODO: Document how the hash table works.
 */
struct hash {
	struct hash	*prev;
	struct hash	*next;
	struct litev_ev  ev;
};

struct hash	**hash_init(void);
void		  hash_free(struct hash ***);

struct hash	 *hash_lookup(struct hash *[], struct litev_ev *);

int		  hash_add(struct hash *[], struct litev_ev *);
void		  hash_del(struct hash *[], struct hash *);

#endif
