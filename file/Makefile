include ../Makefile.inc

LIBS = -lcompat
CFLAGS += -DHAVE_STDINT_H \
          -DHAVE_STRERROR \
          -DHAVE_STRTOUL \
          -DHAVE_UNISTD_H \
          -DHAVE_LOCALE_H \
          -DMAGIC="\"$(CONF_DIR)/magic\""

all: file magic

file: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

magic:
	cat magdir/* > magic

clean:
	rm -f magic file $(OBJECTS)

install: all
	$(INSTALL) -m 755 file $(DESTDIR)/$(BIN_DIR)/file
	$(INSTALL) -m 644 magic $(DESTDIR)/$(CONF_DIR)/magic
	$(INSTALL) -m 644 file.1 $(DESTDIR)/$(MAN_DIR)/man1/file.1
	$(INSTALL) -d -m 755 $(DESTDIR)/$(MAN_DIR)/man5
	$(INSTALL) -m 644 magic.5 $(DESTDIR)/$(MAN_DIR)/man5/magic.5
	$(INSTALL) -d -m 755 $(DESTDIR)/$(DOC_DIR)/file
	$(INSTALL) -m 644 README $(DESTDIR)/$(DOC_DIR)/file/README
	$(INSTALL) -m 644 LEGAL.NOTICE $(DESTDIR)/$(DOC_DIR)/file/LEGAL.NOTICE
	$(INSTALL) -m 644 MAINT $(DESTDIR)/$(DOC_DIR)/file/MAINT
	$(INSTALL) -m 644 COPYING $(DESTDIR)/$(DOC_DIR)/file/COPYING
