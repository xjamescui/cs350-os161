
# Changed so it can be run more easily run out of other locations.
# ROOT=$(HOME)/cs350-os161/root
ROOT=../../root

ASST = 1
SYMLINKS = kernel
KERNSUFFIX = -ASST$(ASST)

setup:
	-ln -s $(ROOT)/kernel$(KERNSUFFIX) kernel

dotar:
	make vclean
	cd ..; tar -czf a$(ASST)-test-scripts.tar.gz `fgrep -v \# a$(ASST)-test-scripts/tarlist`; cd a$(ASST)-test-scripts

runtests:
	./run-batch

clean:
	-/bin/rm $(SYMLINKS)

vclean: clean
	-/bin/rm DISK1.img DISK2.img

