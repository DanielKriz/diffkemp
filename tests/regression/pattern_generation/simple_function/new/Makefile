CC=gcc
PROG=main

SRCS=$(wildcard ./*.c ./**/*.c)
HDRS=$(wildcard ./*.h)
OBJS=$(patsubst %.c,%.o,$(SRCS))

all: $(PROG)

%.o: %.c
	$(CC) $(CFLAGS) $^ -c -o $@

$(PROG): $(OBJS) $(HDRS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

.PHONY: all clean

clean:
	rm -f *.o
	rm -f src/*.o
	rm -f *.zip
	rm -f $(PROG)
