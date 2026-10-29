#!/usr/bin/make

CC = gcc
CFLAGS = -Wall -Wextra -I include

all:
	$(CC) $(CFLAGS) -o hwprobe hwprobe.c
	$(CC) $(CFLAGS) -o kvmprobe kvmprobe.c

hwprobe_test: hwprobe_test.c hwprobe.c
	$(CC) $(CFLAGS) -o $@ hwprobe_test.c

test: hwprobe_test
	./hwprobe_test

check:
	./hwprobe
	./kvmprobe

clean:
	rm -f hwprobe kvmprobe hwprobe_test
