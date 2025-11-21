CC = gcc
CFLAGS = -Wall -O3 -std=c23 -ffast-math -march=native
LDFLAGS = -lm

ASTYLE  := astyle --suffix=none --align-pointer=name --pad-oper

TARGETS := mandelbrot mandelbrot_complex mandelbrot_pthread

SRC     := mandelbrot.c mandelbrot_complex.c mandelbrot_pthread.c
HEADER  :=

.PHONY: all clean fmt

all: $(TARGETS)

$(TARGETS): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

fmt:
	$(ASTYLE) $(SRC) $(HEADER)
