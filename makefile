DEPS = wiz.h

wiz: wiz.c
	cc wiz.c -o wiz $(DEPS) -O2
clean:
	rm wiz
