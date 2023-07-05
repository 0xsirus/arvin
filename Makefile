all:
	gcc -Wall -Wno-varargs -fPIC -shared  -rdynamic -O3 -olibarv.so libarv.c
	gcc -Wall -Wno-varargs -fPIC -shared -O3 -o libarvp.so libarvp.c
	gcc -Wall -Wno-varargs  -oarvin -O3 -rdynamic arvin.c hash.c

install:
	cp libarv.so libarvp.so /lib/x86_64-linux-gnu/
	cp arvin /usr/bin

clean:
	rm arvin libarv.so libarvp.so
