CC ?= cc
PKG_CONFIG ?= pkg-config

CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
          $(shell $(PKG_CONFIG) --cflags jansson pangocairo)

LDLIBS += $(shell $(PKG_CONFIG) --libs jansson pangocairo) -lm

TARGET := build/naturalist-deck

SOURCES := \
    src/main.c \
    src/naturalist.c \
    src/json_handler.c \
    src/search.c \
    src/input.c \
    src/journal.c \
    src/renderer.c

OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build
