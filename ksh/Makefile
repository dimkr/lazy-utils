include ../Makefile.inc

LIBS = -lcompat

OBJECTS = alloc.o c_ksh.o c_sh.o c_test.o c_ulimit.o edit.o emacs.o eval.o \
          exec.o expr.o history.o io.o jobs.o lex.o mail.o main.o misc.o \
          path.o shf.o syn.o table.o trap.o tree.o tty.o var.o version.o vi.o

all: ksh

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

ksh: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f ksh *.o

install: all
	$(INSTALL) -m 755 ksh $(DESTDIR)/$(BIN_DIR)/ksh
	ln -s ksh $(DESTDIR)/$(BIN_DIR)/sh
	$(INSTALL) -m 644 ksh.1 $(DESTDIR)/$(MAN_DIR)/man1/ksh.1
	$(INSTALL) -d -m 755 $(DESTDIR)/$(DOC_DIR)/ksh
	$(INSTALL) -m 644 README $(DESTDIR)/$(DOC_DIR)/ksh/README
	$(INSTALL) -m 644 ChangeLog $(DESTDIR)/$(DOC_DIR)/ksh/ChangeLog
	$(INSTALL) -m 644 ChangeLog.0 $(DESTDIR)/$(DOC_DIR)/ksh/ChangeLog.0
	$(INSTALL) -m 644 CONTRIBUTORS $(DESTDIR)/$(DOC_DIR)/ksh/CONTRIBUTORS
	$(INSTALL) -m 644 LEGAL $(DESTDIR)/$(DOC_DIR)/ksh/LEGAL
