DEPS = wiz.h

wiz: wiz.c
	cc wiz.c -o wiz $(DEPS) -O2

.PHONY: clean install

clean:
	rm wiz

install:
	install wiz /usr/local/bin/wiz
