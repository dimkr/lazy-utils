cd .bin
for i in *
do
	install -D -m 755 $i "$1/bin/$i"
done
cd ../man
for i in *
do
	category="$(echo $i | cut -f 2 -d .)"
	install -D -m 644 $i "$1/usr/share/man/man$category/$i"
done
cd ..
install -D -m 644 README "$1/usr/share/doc/lazy-utils/README"
install -m 644 AUTHORS "$1/usr/share/doc/lazy-utils/AUTHORS"
install -m 644 COPYING "$1/usr/share/doc/lazy-utils/COPYING"
