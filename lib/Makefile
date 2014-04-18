include ../Makefile.inc

CFLAGS += -D_GNU_SOURCE

all: liblazy.a

liblazy.a: $(OBJECTS)
	ar rcs liblazy.a $(OBJECTS)

clean:
	rm -f liblazy.a $(OBJECTS)

install: all
