CCFLAGS = -std=c89 -pedantic -Wall -Werror -Wmissing-prototypes -Wconversion
CCFLAGS += -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align -Wstrict-prototypes 

.PHONY: all
all: release

release: CCFLAGS += -Os
release: um
	@echo Finished [release]

debug: CCFLAGS += -ggdb -O0
debug: um
	@echo Finished [debug]

um: um.o
	@echo Linking
	@echo CCFLAGS: $(CCFLAGS)
	@echo '  [LD] $@'
	@$(CC) $(LDFLAGS) -o um um.o

%.o: %.c
	@echo '  [CC] $@'
	@$(CC) $(CCFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	@rm -rf um
	@rm -rf *.o
	@echo Cleaned up. Done.
