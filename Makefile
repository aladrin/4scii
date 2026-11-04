UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),OpenBSD)
	CC ?= cc
	LDLIBS ?= -lcurses
else
	CC ?= gcc
	LDLIBS ?= -lncurses
endif

CFLAGS ?= -O2 -pipe -Wall -Wextra
CPPFLAGS ?= -DHAVE_CURSES_H

ifeq ($(filter clean distclean,$(MAKECMDGOALS)),)
-include config.mk
endif

TARGET = 4scii
SRCS = 4scii.c canvas.c ui.c fileio.c
OBJS = $(SRCS:.c=.o)
HDRS = 4scii.h

.PHONY: all clean distclean

all: $(TARGET)

config.mk: configure
	sh ./configure

$(TARGET): config.mk $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c $(HDRS) config.mk
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

distclean: clean
	rm -f config.mk
