CC = gcc
CFLAGS = -Wall -Werror -m32

a.out: main.c
	$(CC) $(CFLAGS) -O2 -fpie -pie -o $@ $<

prog/%: prog/%.c
	$(CC) $(CFLAGS) -fno-pie -static -fno-omit-frame-pointer -o $@ $<

clean:
	-rm -f a.out prog/hello prog/segmentfault

.PHONY: clean
