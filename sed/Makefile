include ../Makefile.inc

LIBS = -lcompat

OBJECTS = compile.o main.o misc.o process.o
HEADERS = defs.h extern.h

all: sed

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

sed: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f sed $(OBJECTS)

install: all
	$(INSTALL) -m 755 sed $(DESTDIR)/$(BIN_DIR)/sed
	$(INSTALL) -d -m 644 sed.1 $(DESTDIR)/$(MAN_DIR)/man1/sed.1
