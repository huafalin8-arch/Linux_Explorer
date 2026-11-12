
CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude -D_GNU_SOURCE
LDFLAGS ?=

TARGET := tscope
BUILD  ?= build
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

SRC := $(wildcard src/*.c src/*/*.c)
OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEP)

clean:
	rm -rf $(BUILD) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)