include ../Makefile.inc

LIBS = -lm -lcompat

OBJECTS = b.o lex.o lib.o main.o parse.o proctab.o run.o tran.o ytab.o

all: awk

awk: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f awk $(OBJECTS)

install: all
	$(INSTALL) -m 755 awk $(DESTDIR)/$(BIN_DIR)/awk
	$(INSTALL) -m 644 awk.1 $(DESTDIR)/$(MAN_DIR)/man1/awk.1
	$(INSTALL) -d -m 755 $(DESTDIR)/$(DOC_DIR)/awk
	$(INSTALL) -m 644 README $(DESTDIR)/$(DOC_DIR)/awk/README
