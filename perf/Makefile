.PHONY: all clean

CFLAGS	+= -std=c99 -g -W -Wall -Wextra -Wpedantic -Wmissing-prototypes
CFLAGS	+= -Wstrict-prototypes -Wwrite-strings -Wno-unused-parameter

BINS	 = libevent	\
	   litev

all: perf.o ${BINS}

clean:
	rm -f perf.o ${BINS}

libevent: libevent.c
	${CC} ${CFLAGS} -o $@ perf.o libevent.c -levent

litev: litev.c
	${CC} ${CFLAGS} -I.. -o $@ perf.o litev.c -L.. -litev
