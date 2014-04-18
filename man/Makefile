include ../Makefile.inc

all:

clean:

install: all
	$(INSTALL) -d -m 755 $(DESTDIR)/$(MAN_DIR)/man8
	for page in *.1; do $(INSTALL) -m 644 $$page $(DESTDIR)/$(MAN_DIR)/man1/$$page; done
	for page in *.8; do $(INSTALL) -m 644 $$page $(DESTDIR)/$(MAN_DIR)/man8/$$page; done
