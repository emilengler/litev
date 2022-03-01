.PHONY: all clean install uninstall

PREFIX	 = /usr/local

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

install: all
	${INSTALL} -m 0444 libitev.a ${PREFIX}/lib

uninstall:
	rm -f ${PREFIX}/lib/libitev.a

libitev.a: ${OBJS}
	${AR} rcs $@ ${OBJS}
