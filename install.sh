#!/bin/sh

[ -z "$MANDIR" ] && MANDIR="/usr/share/man"
[ -z "$BINDIR" ] && BINDIR="/bin"
[ -z "$DOCDIR" ] && DOCDIR="/usr/share/doc"

cd .bin
for i in *
do
	install -D -m 755 $i "$1/$BINDIR/$i"
done
cd ../man
for i in *
do
	category="$(echo $i | cut -f 2 -d .)"
	install -D -m 644 $i "$1/$MANDIR/man$category/$i"
done
cd ..
install -D -m 644 README "$1/$DOCDIR/lazy-utils/README"
install -m 644 AUTHORS "$1/$DOCDIR/lazy-utils/AUTHORS"
install -m 644 COPYING "$1/$DOCDIR/lazy-utils/COPYING"
