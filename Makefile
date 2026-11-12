CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -g -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude -D_GNU_SOURCE
LDFLAGS ?=

SRC := $(wildcard src/*.c src/*/*.c)
OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

TARGET := tscope

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEP)

clean:
	rm -f $(OBJ) $(DEP) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall