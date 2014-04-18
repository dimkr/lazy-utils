include ../Makefile.inc

LIBS = -llazy -lcrypt

all: sulogin

sulogin: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f sulogin $(OBJECTS)

install: all
	$(INSTALL) -m 755 sulogin $(DESTDIR)/$(BIN_DIR)/sulogin
	ln -s sulogin $(DESTDIR)/$(BIN_DIR)/login
	ln -s sulogin $(DESTDIR)/$(BIN_DIR)/su
