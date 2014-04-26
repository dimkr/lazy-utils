include ../Makefile.inc

LIBS = -llazy $(shell $(PKG_CONFIG) --libs zlib fuse)
CFLAGS += $(shell $(PKG_CONFIG) --cflags zlib fuse)

all: mergedirs

mergedirs: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f mergedirs $(OBJECTS)

install: all
	$(INSTALL) -m 755 mergedirs $(DESTDIR)/$(BIN_DIR)/mergedirs
	$(INSTALL) -d -m 755 $(DESTDIR)/$(DOC_DIR)/mergedirs
	$(INSTALL) -m 644 COPYING $(DESTDIR)/$(DOC_DIR)/mergedirs/COPYING
