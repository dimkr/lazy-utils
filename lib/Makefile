include ../Makefile.inc

CFLAGS += -D_GNU_SOURCE

all: liblazy.a

liblazy.a: $(OBJECTS)
	ar rcs liblazy.a $(OBJECTS)

clean:
	rm -f liblazy.a $(OBJECTS)

install: all
	$(INSTALL) -d -m 755 $(DESTDIR)/$(DOC_DIR)/crc32.c
	$(INSTALL) -m 644 COPYING.crc32 $(DESTDIR)/$(DOC_DIR)/crc32.c/COPYING
