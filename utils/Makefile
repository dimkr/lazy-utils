include ../Makefile.inc

LIBS = -llazy

PROGS = $(SRCS:.c=)

all: $(PROGS)

$(PROGS): $(OBJECTS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

%: %.o
	$(CC) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
	rm -f $(PROGS) $(OBJECTS)

install: all
	for prog in $(PROGS); do $(INSTALL) -m 755 $$prog $(DESTDIR)/$(BIN_DIR)/$$prog; done
