.PHONY: all clean

CFLAGS	+= -g -W -Wall -Wextra -Wpedantic -Wmissing-prototypes
CFLAGS	+= -Wstrict-prototypes -Wwrite-strings -Wno-unused-parameter

OBJS	 = litev.o

all: libitev.a

clean:
	rm -f libitev.a ${OBJS}

libitev.a: ${OBJS}
	${AR} rcs $@ ${OBJS}
