CFLAGS= -Wall -O0 -g -ggdb -m32
NASMFLAGS= -g -ggdb
LDFLAGS=-static -m32 -L'/usr/lib32'
CC=gcc

ARCH := $(shell uname -m)

BINS=hello justiciero

OBJS=$(BINS:=.o)

all: $(BINS)

# Dependencias
hello.o: hello.c
justiciero.o: justiciero.c

clean:
	rm -rf $(OBJS) $(BINS)
