include ../Makefile.inc

CFLAGS += -DHAVE_STDINT_H=1 -DHAVE_SYS_SYSCTL_H=0
OBJECTS = arc4random.o fgetln.o

all: libcompat.a

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

libcompat.a: $(OBJECTS)
	ar rcs libcompat.a $(OBJECTS)

clean:
	rm -f libcompat.a $(OBJECTS)

install: all
	$(INSTALL) -d -m 755 $(DESTDIR)/$(DOC_DIR)/arc4random
	$(INSTALL) -m 644 README.arc4random $(DESTDIR)/$(DOC_DIR)/arc4random/README
	$(INSTALL) -m 644 COPYING.arc4random $(DESTDIR)/$(DOC_DIR)/arc4random/COPYING
	$(INSTALL) -d -m 755 $(DESTDIR)/$(DOC_DIR)/queue.h
	$(INSTALL) -m 644 COPYING.queue $(DESTDIR)/$(DOC_DIR)/queue.h/COPYING
