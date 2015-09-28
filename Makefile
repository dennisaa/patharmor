dirs = lkm dyninst-pass dyninst-static shared bin di-opt toy-bug at

all:
	for d in $(dirs); do make -C $$d || exit 1; done

install: all
	for d in $(dirs); do make -C $$d install || exit 1; done

clean:
	for d in $(dirs); do make -C $$d clean || exit 1; done
