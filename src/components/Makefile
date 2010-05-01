# Order matters here.  Interface files rely on library files, and
# component implementations rely on both the interfaces and the libs

MAKEFLAGS=-I$(shell pwd)

all:
	make $(MAKEFLAGS) -C lib
	make $(MAKEFLAGS) -C interface
	make $(MAKEFLAGS) -C implementation

cp:
	make $(MAKEFLAGS) -C implementation cp

clean:
	make $(MAKEFLAGS) -C lib clean
	make $(MAKEFLAGS) -C interface clean
	make $(MAKEFLAGS) -C implementation clean

init:
	make $(MAKEFLAGS) -C lib init
	make $(MAKEFLAGS) -C interface init
	make $(MAKEFLAGS) -C implementation init