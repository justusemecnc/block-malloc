CC ?= gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS =

ifeq ($(OS),Windows_NT)
    EXE = block_malloc_test.exe
    RM = del /Q
else
    EXE = block_malloc_test
    RM = rm -f
endif

OBJS = block_malloc.o test.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

block_malloc.o: block_malloc.c block_malloc.h
	$(CC) $(CFLAGS) -c block_malloc.c

test.o: test.c block_malloc.h
	$(CC) $(CFLAGS) -c test.c

test: $(EXE)
ifeq ($(OS),Windows_NT)
	.\$(EXE)
else
	./$(EXE)
endif

clean:
	$(RM) $(OBJS) $(EXE) 2>nul || true

.PHONY: all test clean
