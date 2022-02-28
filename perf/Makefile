.PHONY: all clean

CFLAGS	+= -std=c99 -g -W -Wall -Wextra -Wpedantic -Wmissing-prototypes
CFLAGS	+= -Wstrict-prototypes -Wwrite-strings -Wno-unused-parameter

BINS	 = libevent

all: libevent

clean:
	rm -f ${BINS}

libevent: libevent.c
	${CC} ${CFLAGS} -O2 -o $@ libevent.c -levent
