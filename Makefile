CC ?= cc
CFLAGS ?= -O2 -pipe
LDFLAGS ?= -s
DESTDIR ?=
BIN_DIR ?= /bin
SBIN_DIR ?= /sbin
MAN_DIR ?= /usr/share/man
DOC_DIR ?= /usr/share/doc

CFLAGS += -std=c99 -D_GNU_SOURCE -Wall -pedantic -Werror=uninitialized

INSTALL = install -v

SRCS = $(wildcard *.c)
OBJECTS = $(SRCS:.c=.o)
HEADERS = $(wildcard *.h)
PROGS = init poweroff reboot suspend cttyhack syslogd klogd modprobed modprobe \
        devd losetup mount umount tftpd odus

all: $(PROGS)

%.o: %.c $(HEADERS)
	$(CC) -c -o $@ $< $(CFLAGS)

init: daemon.o init.o
	$(CC) -o $@ $^ $(LDFLAGS)

poweroff: poweroff.o
	$(CC) -o $@ $^ $(LDFLAGS)

reboot: reboot.o
	$(CC) -o $@ $^ $(LDFLAGS)

suspend: suspend.o
	$(CC) -o $@ $^ $(LDFLAGS)

cttyhack: cttyhack.o
	$(CC) -o $@ $^ $(LDFLAGS)

syslogd: daemon.o syslogd.o
	$(CC) -o $@ $^ $(LDFLAGS)

klogd: daemon.o klogd.o
	$(CC) -o $@ $^ $(LDFLAGS)

modprobed: daemon.o module.o find.o cache.o modprobed.o
	$(CC) -o $@ $^ $(LDFLAGS)

modprobe: module.o modprobe.o
	$(CC) -o $@ $^ $(LDFLAGS)

devd: daemon.o find.o devd.o
	$(CC) -o $@ $^ $(LDFLAGS)

losetup: losetup.o
	$(CC) -o $@ $^ $(LDFLAGS)

mount: mount.o
	$(CC) -o $@ $^ $(LDFLAGS)

umount: umount.o
	$(CC) -o $@ $^ $(LDFLAGS)

tftpd: daemon.o tftpd.o
	$(CC) -o $@ $^ $(LDFLAGS)

odus: odus.o
	$(CC) -o $@ $^ $(LDFLAGS)

install: all
	$(INSTALL) -D -m 755 init $(DESTDIR)/$(SBIN_DIR)/init
	$(INSTALL) -D -m 755 poweroff $(DESTDIR)/$(SBIN_DIR)/poweroff
	$(INSTALL) -D -m 755 reboot $(DESTDIR)/$(SBIN_DIR)/reboot
	$(INSTALL) -D -m 755 cttyhack $(DESTDIR)/$(SBIN_DIR)/cttyhack
	$(INSTALL) -D -m 755 syslogd $(DESTDIR)/$(SBIN_DIR)/syslogd
	$(INSTALL) -D -m 755 klogd $(DESTDIR)/$(SBIN_DIR)/klogd
	$(INSTALL) -D -m 755 modprobed $(DESTDIR)/$(SBIN_DIR)/modprobed
	$(INSTALL) -D -m 755 modprobe $(DESTDIR)/$(SBIN_DIR)/modprobe
	$(INSTALL) -D -m 755 devd $(DESTDIR)/$(SBIN_DIR)/devd
	$(INSTALL) -D -m 755 losetup $(DESTDIR)/$(SBIN_DIR)/losetup
	$(INSTALL) -D -m 755 mount $(DESTDIR)/$(BIN_DIR)/mount
	$(INSTALL) -D -m 755 umount $(DESTDIR)/$(BIN_DIR)/umount
	$(INSTALL) -D -m 755 tftpd $(DESTDIR)/$(SBIN_DIR)/tftpd
	$(INSTALL) -D -m 755 odus $(DESTDIR)/$(BIN_DIR)/odus

	$(INSTALL) -D -m 644 init.8 $(DESTDIR)/$(MAN_DIR)/man8/init.8
	$(INSTALL) -D -m 644 poweroff.8 $(DESTDIR)/$(MAN_DIR)/man8/poweroff.8
	$(INSTALL) -D -m 644 reboot.8 $(DESTDIR)/$(MAN_DIR)/man8/reboot.8
	$(INSTALL) -D -m 644 cttyhack.1 $(DESTDIR)/$(MAN_DIR)/man1/cttyhack.1
	$(INSTALL) -D -m 644 syslogd.8 $(DESTDIR)/$(MAN_DIR)/man8/syslogd.8
	$(INSTALL) -D -m 644 klogd.8 $(DESTDIR)/$(MAN_DIR)/man8/klogd.8
	$(INSTALL) -D -m 644 modprobed.8 $(DESTDIR)/$(MAN_DIR)/man8/modprobed.8
	$(INSTALL) -D -m 644 modprobe.8 $(DESTDIR)/$(MAN_DIR)/man8/modprobe.8
	$(INSTALL) -D -m 644 devd.8 $(DESTDIR)/$(MAN_DIR)/man8/devd.8
	$(INSTALL) -D -m 644 losetup.8 $(DESTDIR)/$(MAN_DIR)/man8/losetup.8
	$(INSTALL) -D -m 644 mount.8 $(DESTDIR)/$(MAN_DIR)/man8/mount.8
	$(INSTALL) -D -m 644 umount.8 $(DESTDIR)/$(MAN_DIR)/man8/umount.8
	$(INSTALL) -D -m 644 tftpd.8 $(DESTDIR)/$(MAN_DIR)/man8/tftpd.8
	$(INSTALL) -D -m 644 odus.8 $(DESTDIR)/$(MAN_DIR)/man8/odus.8

	$(INSTALL) -D -m 644 README $(DESTDIR)/$(DOC_DIR)/lazy-utils/README
	$(INSTALL) -m 644 AUTHORS $(DESTDIR)/$(DOC_DIR)/lazy-utils/AUTHORS
	$(INSTALL) -m 644 COPYING $(DESTDIR)/$(DOC_DIR)/lazy-utils/COPYING

	$(INSTALL) -m 755 -d $(DESTDIR)/srv/tftp

clean:
	rm -f $(PROGS) $(OBJECTS)
