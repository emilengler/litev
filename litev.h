#ifndef LITEV_H
#define LITEV_H

#ifdef __cplusplus
extern "C" {
#endif

#define LITEV_READ	1
#define LITEV_WRITE	2

struct litev_base;
struct litev_ev;

struct litev_ev {
	int	  fd;
	short	  condition;
	void	(*cb)(struct litev_base *, int, short, void *);
	void	 *udata;
};

struct litev_base	*litev_init(void);
void			 litev_free(struct litev_base **);

int			 litev_dispatch(struct litev_base *);
int			 litev_break(struct litev_base *);

int			 litev_add(struct litev_base *, struct litev_ev *);
int			 litev_del(struct litev_base *, struct litev_ev *);

#ifdef __cplusplus
}
#endif

#endif
