include ../Makefile.inc

LIBS = -llazy $(shell $(PKG_CONFIG) --libs zlib)
CFLAGS += $(shell $(PKG_CONFIG) --cflags zlib)

PROGS = $(SRCS:.c=)

all: $(PROGS)

$(PROGS): $(OBJECTS)

%: %.o
	$(CC) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	rm -f $(PROGS) $(OBJECTS)

install: all
	for prog in $(PROGS); do $(INSTALL) -m 755 $$prog $(DESTDIR)/$(BIN_DIR)/$$prog; done
