CC     ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
SRC     = src/heap.c src/graph.c src/routing.c src/sample.c

all: navmap

navmap: src/main.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

tests/test_navigation: tests/test_navigation.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ -lm

test: tests/test_navigation
	./tests/test_navigation

clean:
	rm -f navmap tests/test_navigation

.PHONY: all test clean
