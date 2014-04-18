include ../Makefile.inc

LIBS = -lcompat

all: sed

sed: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f sed $(OBJECTS)

install: all
	$(INSTALL) -m 755 sed $(DESTDIR)/$(BIN_DIR)/sed
	$(INSTALL) -m 644 sed.1 $(DESTDIR)/$(MAN_DIR)/man1/sed.1
	$(INSTALL) -d -m 755 $(DESTDIR)/$(DOC_DIR)/sed
	$(INSTALL) -m 644 README $(DESTDIR)/$(DOC_DIR)/sed/README
	$(INSTALL) -m 644 COPYING $(DESTDIR)/$(DOC_DIR)/sed/COPYING
