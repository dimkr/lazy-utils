DESTDIR ?= /
CONF_DIR ?= /etc
BIN_DIR ?= /bin
MAN_DIR ?= /usr/share/man
DOC_DIR ?= /usr/share/doc/lazy-utils
CC ?= cc
CFLAGS ?= -Os 
CFLAGS += -Wall -pedantic --std=c99 -D_GNU_SOURCE \
          -I../compat/include -I../lib/include \
          -DCONF_DIR="\"$(CONF_DIR)\""
PKG_CONFIG ?= pkg-config
LDFLAGS ?= -s
LDFLAGS += -L../compat -L../lib
INSTALL = install -v

SRCS = $(wildcard *.c)
OBJECTS = $(SRCS:.c=.o)
HEADERS = $(wildcard *.h)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)
