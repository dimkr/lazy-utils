include Makefile.inc

all: .ksh .awk .sed .file .utils .sulogin .man

.compat:
	cd compat; $(MAKE)

.lib:
	cd lib; $(MAKE)

.ksh: .compat
	cd ksh; $(MAKE)

.awk: .compat
	cd awk; $(MAKE)

.sed: .compat
	cd sed; $(MAKE)

.file: .compat
	cd file; $(MAKE)

.utils: .lib
	cd utils; $(MAKE)

.sulogin: .lib
	cd sulogin; $(MAKE)

.man:
	cd man; $(MAMKE)

install: all
	$(INSTALL) -D -d -m 755 $(DESTDIR)/$(BIN_DIR)
	$(INSTALL) -D -d -m 755 $(DESTDIR)/$(MAN_DIR)/man1
	$(INSTALL) -D -d -m 755 $(DESTDIR)/$(CONF_DIR)
	$(INSTALL) -D -d -m 755 $(DESTDIR)/$(DOC_DIR)
	cd man; $(MAKE) install
	cd sulogin; $(MAKE) install
	cd utils; $(MAKE) install
	cd lib; $(MAKE) install
	cd file; $(MAKE) install
	cd sed; $(MAKE) install
	cd awk; $(MAKE) install
	cd ksh; $(MAKE) install
	cd compat; $(MAKE) install
	$(INSTALL) -m 644 README $(DESTDIR)/$(DOC_DIR)/README
	$(INSTALL) -m 644 AUTHORS $(DESTDIR)/$(DOC_DIR)/AUTHORS
	$(INSTALL) -m 644 COPYING $(DESTDIR)/$(DOC_DIR)/COPYING

clean:
	cd man; $(MAKE) clean
	cd sulogin; $(MAKE) clean
	cd utils; $(MAKE) clean
	cd lib; $(MAKE) clean
	cd file; $(MAKE) clean
	cd sed; $(MAKE) clean
	cd awk; $(MAKE) clean
	cd ksh; $(MAKE) clean
	cd compat; $(MAKE) clean
