
# This is the defs.mk that is found in the os161-1.11 directory.
include ../defs.mk

# This is relative to the $(HOME) directory which doesn't work
# when running as part of cs350_submit 
# The include above sets this OSTREE variable and we just use that.
# ROOT=$(OSTREE)

# Changed so it can be run out of other locations.
# ROOT=$(HOME)/cs350-os161/root

# Needs to be a relative path
ROOT=../../root

SYMLINKS = testbin uw-testbin bin kernel my-testbin
GENERATED = bigfile.out tailfile catfile hashfile badcallfile
TESTOUTPUT = out.A3 out.A3-option1 out.A3-option2
ASST = -ASST3

setup:
	-ln -s $(ROOT)/testbin .
	-ln -s $(ROOT)/uw-testbin .
	-ln -s $(ROOT)/my-testbin .
	-ln -s $(ROOT)/bin .
	-ln -s $(ROOT)/kernel$(ASST) kernel

all: setup runtests

runtests: 
	./run-batch-A3-all
	-/bin/rm -f SWAPFILE

dotar:
	make vclean
	cd ..; tar --exclude CVS -czf a3-test-scripts.tar.gz `cat a3-test-scripts/tarlist`; cd a3-test-scripts

clean:
	-/bin/rm $(SYMLINKS)
	-/bin/rm $(GENERATED)

vclean: clean
	-/bin/rm $(TESTOUTPUT)
	-/bin/rm DISK1.img DISK2.img
	-/bin/rm -rf newtests
	-/bin/rm tmp.got tmp.expected
	-/bin/rm out.A3*
	-/bin/rm SWAPFILE

