CC = clang
CFLAGS = -Wall -O3 -std=c23 -ffast-math -march=native -DNDEBUG
#CFLAGS = -Wall -O0 -std=c23 -g -fsanitize=address -fsanitize=thread
LDFLAGS = -lm

TARGETS := mandelbrot mandelbrot_complex mandelbrot_pthread mandelbrot_simd_pthread mandelbrot_simd_pthread_v8

SRC     := mandelbrot.c mandelbrot_complex.c mandelbrot_pthread.c  mandelbrot_simd_pthread.c mandelbrot_simd_pthread_v8.c
HEADER  :=

.PHONY: all clean fmt

all: $(TARGETS)

$(TARGETS): %: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

fmt:
	astyle --suffix=none --align-pointer=name --pad-oper $(SRC) $(HEADER)
