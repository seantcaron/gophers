</$objtype/mkfile

gophers:	gophers.$0
		$LD $LDFLAGS -o $target gophers.8

%.$0:	%.c
		$CC $CFLAGS $stem.c
