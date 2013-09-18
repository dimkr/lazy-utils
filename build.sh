BASE_DIR="$(pwd)"
[ -z "$CC" ] && CC="cc"
[ -z "$CFLAGS" ] && CFLAGS="-Os -mtune=generic"
CFLAGS="-I$BASE_DIR/include -Wall -pedantic -D_GNU_SOURCE $CFLAGS"

echo "Building lazy-utils with $CC $CFLAGS"

# build all source files
for i in $(find -type f -name '*.c')
do
	echo "Compiling $i"
	$CC -c $CFLAGS $i -o $(echo $i | sed s/\.c$/.o/)
done

# generate a static library
ar rcs liblazy.a lib/*.o

if [ -d .bin ]
then
	rm -vf .bin/*
else
	mkdir .bin
fi

# link all applets
for i in $(find $BASE_DIR -type f -name '*.o')
do
	applet_name="$(basename $i .o)"

	case "$i" in
		$BASE_DIR/lib/*)
			continue
			;;

		$BASE_DIR/login/*)
			libraries="-lcrypt -llazy"
			;;

		*)
			libraries="-llazy"
			;;
	esac

	echo "Linking $applet_name with: $libraries"

	$CC $i -L. $libraries -o .bin/$applet_name
done
