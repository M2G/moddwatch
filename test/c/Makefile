# Makefile for C bridge tests
# Usage: make test

CC      = gcc

# Allow overriding from environment
CFLAGS  ?= -I../.. -I/opt/homebrew/include -Wall -Wextra -g
LDFLAGS ?= -L/opt/homebrew/lib -lfswatch -lpthread

# Linux defaults
ifeq ($(shell uname -s), Linux)
    CFLAGS  ?= -I../.. -I/usr/local/include -Wall -Wextra -g
    LDFLAGS ?= -L/usr/local/lib -lfswatch -lstdc++ -lm -lpthread
endif

SRCS = test_bridge.c ../../fswatch_bridge.c
BIN  = test_bridge

.PHONY: test clean

test: $(BIN)
	./$(BIN)

$(BIN): $(SRCS)
	$(CC) $(CFLAGS) -I../.. $(SRCS) $(LDFLAGS) -o $(BIN)

clean:
	rm -f $(BIN)