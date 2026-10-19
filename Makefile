CC ?= cc
CPPFLAGS ?= -Isrc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror

.PHONY: all test clean sanitize
all: build/process-inspector

build:
	mkdir -p build

build/process-inspector: src/main.c src/inspector.c src/inspector.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/inspector.c -o $@

build/test-inspector: tests/test_inspector.c src/inspector.c src/inspector.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_inspector.c src/inspector.c -o $@

test: all build/test-inspector
	./build/test-inspector
	python3 tests/test_cli.py

sanitize:
	$(MAKE) clean
	$(MAKE) test CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer'

clean:
	rm -rf build
