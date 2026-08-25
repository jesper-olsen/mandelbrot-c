# Mandelbrot in C

This repository contains a modern C (C99) implementation for generating visualizations of the Mandelbrot set. 

The program compiles to a single native executable. It can render the Mandelbrot set directly to the terminal as ASCII art or produce a data file for `gnuplot` to generate a high-resolution PNG image.

### Other Language Implementations

This project is part of a suite of mandelbrot implementations in different languages.

Single Thread/Multi-thread shows the number of seconds it takes to do a 5000x5000 calculation.


| Language    | Repository                                                            | Single Thread   | Multi-Thread | Simd | Multi-Thread + Simd |
| :--------   | :-------------------------------------------------------------------- | ---------------:| -----------: | ----:| ------------------: |
| Awk         | [mandelbrot-awk](https://github.com/jesper-olsen/mandelbrot-awk)      |           417.9 |              |      |                     |
| **C**       | [mandelbrot-c](https://github.com/jesper-olsen/mandelbrot-c)          |             3.6 |          0.6 |  0.7 |               0.2   |
| Erlang      | [mandelbrot_erl](https://github.com/jesper-olsen/mandelbrot_erl)      |                 |              |      |                     |
| Fortran     | [mandelbrot-f](https://github.com/jesper-olsen/mandelbrot-f)          |                 |              |      |                     |
| Java        | [mandelbrot-java](https://github.com/jesper-olsen/mandelbrot-java)    |             3.9 |          0.8 |  1.4 |               0.5   |
| Lua         | [mandelbrot-lua](https://github.com/jesper-olsen/mandelbrot-lua)      |            33.2 |              |      |                     |
| Mojo        | [mandelbrot-mojo](https://github.com/jesper-olsen/mandelbrot-mojo)    |             3.8 |          1.2 |  0.7 |               0.4   |
| Nushell     | [mandelbrot-nu](https://github.com/jesper-olsen/mandelbrot-nu)        |                 |              |      |                     |
| Odin        | [mandelbrot-odin](https://github.com/jesper-olsen/mandelbrot-odin)    |             4.4 |              |      |                     |
| Python      | [mandelbrot-py](https://github.com/jesper-olsen/mandelbrot-py)        |     (pure) 93.3 | (jax)    5.9 |      |                     |
| R           | [mandelbrot-R](https://github.com/jesper-olsen/mandelbrot-R)          |                 |              |      |                     |
| Rust        | [mandelbrot-rs](https://github.com/jesper-olsen/mandelbrot-rs)        |             4.7 |          1.3 |      |                     |
| Swift       | [mandelbrot-swift](https://github.com/jesper-olsen/mandelbrot-swift)  |             4.5 |              |      |                     |
| Tcl         | [mandelbrot-tcl](https://github.com/jesper-olsen/mandelbrot-tcl)      |                 |              |      |                     |
| Zig         | [mandelbrot-zig](https://github.com/jesper-olsen/mandelbrot-zig)      |             4.9 |          0.9 |  0.7 |               0.3   |






---

## Prerequisites

You will need the following installed:

1.  A **C Compiler** (e.g., GCC or Clang).
2.  **Make** (optional, but recommended for easy building).
3.  **Gnuplot** (required *only* for generating PNG images).

---

## Build

You can compile the program directly or use the provided Makefile.

**Option 1: Manual Compilation**
The `-lm` flag links the math library, and `-O3` enables aggressive optimizations.

```sh
gcc -o mandelbrot mandelbrot.c -lm -O3
```

**Option 2: Using Make**
Simply run `make` in the project directory.

```sh
make
```

---

## Usage

The compiled executable can be configured via command-line arguments using a `key=value` format.

### 1. ASCII Art Output

To render the Mandelbrot set directly in your terminal, run the executable.

```sh
./mandelbrot
```

You can change the view and resolution by passing parameters:
```sh
# Zoom in on a different area with a wider view
./mandelbrot width=120 ll_x=-0.75 ll_y=0.1 ur_x=-0.74 ur_y=0.11
```

### 2. PNG Image Generation

To create a high-resolution PNG, you first generate a data file and then process it with `gnuplot`.

**Step 1: Generate the data file**
Set `png=1` and specify the desired dimensions. Redirect the output to a file.

```sh
./mandelbrot png=1 width=1000 height=750 > image.dat
```

**Step 3: Run gnuplot**
This will read `image.dat` and create `mandelbrot.png`.

```sh
gnuplot topng.gp
```
The result is a high-quality `mandelbrot.png` image.

![PNG Image of the Mandelbrot Set](mandelbrot.png)

## Performance

Benchmarks were run on an **Apple M5** system with Apple clang version 21.0.0 

**Generating a 1000x750 data file:**
```sh
time ./mandelbrot png=1 width=1000 height=750 > image.dat
0.30s user 0.01s system 96% cpu 0.329 total
```

**Generating a 5000x5000 data file:**
```sh
time ./mandelbrot png=1 width=5000 height=5000 > image.dat
3.57s user 0.04s system 99% cpu 3.625 total
```

**Generating a 5000x5000 data file multiple worker threads**
```sh
time ./mandelbrot_pthread  png=1 width=5000 height=5000 > image.dat
5.11s user 0.06s system 884% cpu 0.584 total
```

**Generating a 5000x5000 data file with SIMD and multiple worker threads:**
```sh
time ./mandelbrot_simd_pthread_v8 png=1 width=5000 height=5000 > image.dat
0.95s user 0.05s system 631% cpu 0.160 total
```
**Generating a 20000x20000 data file with SIMD and multiple worker threads:**
```sh
time ./mandelbrot_simd_pthread_v8 png=1 width=20000 height=20000 > image.dat
13.51s user 0.47s system 679% cpu 2.057 total
```


