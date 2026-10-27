#!/usr/bin/make

all:
	gcc -I include -o hwprobe hwprobe.c
	gcc -I include -o kvmprobe kvmprobe.c

check:
	./hwprobe
	./kvmprobe

clean:
	rm -f hwprobe kvmprobe
