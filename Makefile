CCFLAGS = -std=c89 -pedantic -Wall -Werror -Wmissing-prototypes -Wconversion
CCFLAGS += -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wstrict-prototypes 

.PHONY: all
all: release

release: um

debug: um
debug: CCFLAGS += -ggdb -O0

um: um.o
	$(CC) $(LDFLAGS) -o um um.o

%.o: %.c
	$(CC) $(CCFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm -rf um
	rm -rf *.o
