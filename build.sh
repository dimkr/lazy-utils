BASE_DIR="$(pwd)"
CFLAGS="-I$BASE_DIR/include $CFLAGS"

echo "Building lazy-utils with $CC $CFLAGS and $LD $LDFLAGS"

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
			LIBS="-lcrypt -llazy"
			;;

		*)
			LIBS="-llazy"
			;;
	esac

	echo "Linking $applet_name with: $LIBS"

	$CC $i -L. $LDFLAGS $LIBS -o .bin/$applet_name
done
