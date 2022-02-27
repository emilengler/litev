.PHONY: all clean

CFLAGS	+= -std=c99 -g -W -Wall -Wextra -Wpedantic -Wmissing-prototypes
CFLAGS	+= -Wstrict-prototypes -Wwrite-strings -Wno-unused-parameter

OBJS	 = litev.o	\
	   hash.o	\
	   kqueue.o	\
	   epoll.o	\
	   poll.o

all: libitev.a

clean:
	rm -f libitev.a ${OBJS}

libitev.a: ${OBJS}
	${AR} rcs $@ ${OBJS}
