cd .bin
for i in *
do
	install -D -m 755 $i "$1/bin/$i"
done
ln -s sulogin "$1/bin/su"
ln -s sulogin "$1/bin/login"
cd ..
install -D -m 644 README "$1/usr/share/doc/lazy-utils/README"
install -m 644 AUTHORS "$1/usr/share/doc/lazy-utils/AUTHORS"
install -m 644 COPYING "$1/usr/share/doc/lazy-utils/COPYING"
