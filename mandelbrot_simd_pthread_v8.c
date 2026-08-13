/**
 * @file mandelbrot_simd_pthread.c
 * @brief Generates Mandelbrot set visualizations in ASCII or for gnuplot,
 *        using SIMD (8-wide, via portable clang/gcc vector extensions) and
 *        pthreads with dynamic work-stealing (atomic row counter).
 *
 * A modern C implementation for a cross-language comparison project.
 * It parses command-line arguments in the format key=value.
 *
 * The SIMD kernel uses float32 instead of the scalar path's float64, which
 * gives ~1% of pixels an iteration count off by more than one compared to
 * the scalar/double-precision result - a precision tradeoff, not a bug.
 *
 * Compilation:
 * gcc -o mandelbrot_simd_pthread mandelbrot_simd_pthread_v8.c -lm -O3 -pthread
 * clang -o mandelbrot_simd_pthread mandelbrot_simd_pthread_v8.c -lm -O3 -pthread
 *
 * Usage:
 * ./mandelbrot_simd_pthread_v8
 * ./mandelbrot_simd_pthread_v8 width=120 ll_x=-0.75 ll_y=0.1 ur_x=-0.74 ur_y=0.11
 * ./mandelbrot_simd_pthread_v8 png=1 width=800 height=600 > mandelbrot.dat
 */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define NUM_THREADS 10 // Adjust based on CPU core count
#define CHUNK_SIZE 1 // Number of rows to process per task

atomic_int global_next_y = 0; // threads get the next row to process from this global atomic

typedef struct {
    int width;
    int height;
    bool png;
    double ll_x;
    double ll_y;
    double ur_x;
    double ur_y;
    int max_iter;
} Config;

/* 4-wide float32 / int32 vectors (portable clang/gcc extension - compiles
 * to NEON on arm64, SSE/AVX on x86, no target-specific intrinsics needed). */
//typedef float v4f __attribute__((vector_size(16)));
//typedef int32_t v4i __attribute__((vector_size(16)));
typedef float v8f __attribute__((vector_size(32)));
typedef int32_t v8i __attribute__((vector_size(32)));

/**
 * @brief Maps an iteration count to an ASCII character.
 * @param value The iteration value (0 to max_iter).
 * @param max_iter The maximum number of iterations.
 * @return A character for visualization.
 */
char cnt2char(int value, int max_iter) {
    const char *symbols = "MW2a_. ";
    int ns = strlen(symbols);
    // Map the value [0, max_iter] to an index [0, ns-1]
    int idx = (int)((double)value / max_iter * (ns - 1));
    return symbols[idx];
}

/**
 * @brief Calculates the escape time for a point in the complex plane.
 * @param cr The real part of the complex number c.
 * @param ci The imaginary part of the complex number c.
 * @param max_iter The maximum number of iterations.
 * @return An integer representing how close the point is to the set.
 */
static inline int escape_time(double cr, double ci, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int iter;
    for (iter = 0; iter < max_iter; ++iter) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            break;
        }
        double tmp = zr2 - zi2 + cr;
        zi = 2.0 * zr * zi + ci;
        zr = tmp;
    }
    return max_iter - iter;
}

/**
 * @brief Calculates the escape time for 8 points at once.
 * @param cr The real parts of 8 complex numbers.
 * @param ci The imaginary parts of 8 complex numbers.
 * @param max_iter The maximum number of iterations.
 * @return 4 escape-time counts, one per lane.
 *
 * Can't use a data-dependent `break` per lane the way the scalar version
 * does - instead, every lane keeps iterating, and a mask (`active`) stops
 * lanes from accumulating further iterations once they've escaped, until
 * either every lane has escaped or max_iter is reached.
 */

static inline v8i escape_time_simd8(v8f cr, v8f ci, int max_iter) {
    v8f zr = {0, 0, 0, 0, 0, 0, 0, 0};
    v8f zi = {0, 0, 0, 0, 0, 0, 0, 0};
    v8i iters = {0, 0, 0, 0, 0, 0, 0, 0};
    v8i active = {-1, -1, -1, -1, -1, -1, -1, -1}; // all bits set = "still iterating"

    for (int i = 0; i < max_iter; ++i) {
        v8f zr2 = zr * zr;
        v8f zi2 = zi * zi;
        v8i still = (zr2 + zi2 <= 4.0f);
        active &= still;

        if ((active[0] | active[1] | active[2] | active[3] |
                active[4] | active[5] | active[6] | active[7]) == 0)
            break;

        v8f tmp = zr2 - zi2 + cr;
        zi = 2.0f * zr * zi + ci;
        zr = tmp;

        iters -= active; // active lanes are -1 (all bits set): iters -= -1 == iters += 1
    }

    v8i max_iter_v = {max_iter, max_iter, max_iter, max_iter,
                      max_iter, max_iter, max_iter, max_iter
                     };
    return max_iter_v - iters;
}


/**
 * @brief Parses a single "key=value" command-line argument.
 * @param arg The string argument from argv.
 * @param config A pointer to the configuration struct to be updated.
 */
void parse_arg(char *arg, Config *config) {
    char *value = strchr(arg, '=');
    if (value == NULL) {
        fprintf(stderr, "Warning: Ignoring invalid argument '%s'\n", arg);
        return;
    }
    *value = '\0'; // Temporarily split string into key and value
    value++;       // Move pointer to the start of the value part

    if (strcmp(arg, "width") == 0) config->width = atoi(value);
    else if (strcmp(arg, "height") == 0) config->height = atoi(value);
    else if (strcmp(arg, "png") == 0) config->png = (bool)atoi(value);
    else if (strcmp(arg, "ll_x") == 0) config->ll_x = atof(value);
    else if (strcmp(arg, "ll_y") == 0) config->ll_y = atof(value);
    else if (strcmp(arg, "ur_x") == 0) config->ur_x = atof(value);
    else if (strcmp(arg, "ur_y") == 0) config->ur_y = atof(value);
    else if (strcmp(arg, "max_iter") == 0) config->max_iter = atoi(value);
    else fprintf(stderr, "Warning: Unknown parameter '%s'\n", arg);

    *(value - 1) = '='; // Restore the original argument string
}

typedef struct {
    int start_y;
    int end_y;
    const Config *config;
    int *output_buffer; // Pointer to the result array
} ThreadArgs;

// Process image row by row - threads get their next job from @global_next_y
void *thread_mandelbrot(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    const Config *config = args->config;
    int *buffer = args->output_buffer;
    double fwidth = config->ur_x - config->ll_x;
    double fheight = config->ur_y - config->ll_y;

    // Loop to get task chunks until the work is done
    while (true) {
        int y_start = atomic_fetch_add(&global_next_y, CHUNK_SIZE);
        int y_end = y_start + CHUNK_SIZE;
        if (y_start >= config->height) {
            break;
        }
        if (y_end > config->height) {
            y_end = config->height;
        }

        for (int y = y_start; y < y_end; ++y) {
            double imag = config->ur_y - y * fheight / config->height;
            float imag_f = (float)imag;

            int x = 0;
            //for (; x + 4 <= config->width; x += 4) {
            //    v4f cr = {
            //        (float)(config->ll_x + (x + 0) * fwidth / config->width),
            //        (float)(config->ll_x + (x + 1) * fwidth / config->width),
            //        (float)(config->ll_x + (x + 2) * fwidth / config->width),
            //        (float)(config->ll_x + (x + 3) * fwidth / config->width),
            //    };
            //    v4f ci = {imag_f, imag_f, imag_f, imag_f};
            //    v4i vals = escape_time_simd4(cr, ci, config->max_iter);
            //    buffer[y * config->width + x + 0] = vals[0];
            //    buffer[y * config->width + x + 1] = vals[1];
            //    buffer[y * config->width + x + 2] = vals[2];
            //    buffer[y * config->width + x + 3] = vals[3];
            //}
            for (; x + 8 <= config->width; x += 8) {
                v8f cr = {
                    (float)(config->ll_x + (x + 0) * fwidth / config->width),
                    (float)(config->ll_x + (x + 1) * fwidth / config->width),
                    (float)(config->ll_x + (x + 2) * fwidth / config->width),
                    (float)(config->ll_x + (x + 3) * fwidth / config->width),
                    (float)(config->ll_x + (x + 4) * fwidth / config->width),
                    (float)(config->ll_x + (x + 5) * fwidth / config->width),
                    (float)(config->ll_x + (x + 6) * fwidth / config->width),
                    (float)(config->ll_x + (x + 7) * fwidth / config->width),
                };
                v8f ci = {imag_f, imag_f, imag_f, imag_f, imag_f, imag_f, imag_f, imag_f};
                v8i vals = escape_time_simd8(cr, ci, config->max_iter);
                buffer[y * config->width + x + 0] = vals[0];
                buffer[y * config->width + x + 1] = vals[1];
                buffer[y * config->width + x + 2] = vals[2];
                buffer[y * config->width + x + 3] = vals[3];
                buffer[y * config->width + x + 4] = vals[4];
                buffer[y * config->width + x + 5] = vals[5];
                buffer[y * config->width + x + 6] = vals[6];
                buffer[y * config->width + x + 7] = vals[7];
            }

            // Remainder columns that don't fill a full 4-wide chunk.
            for (; x < config->width; ++x) {
                double real = config->ll_x + x * fwidth / config->width;
                int iter = escape_time(real, imag, config->max_iter);
                buffer[y * config->width + x] = iter;
            }
        }
    }
    return NULL;
}

void final_output(const Config *config, const int *result_buffer) {
    // Calculate buffer size for one row:
    // Max 3 digits + 2 chars (", ") = 5 bytes/pixel. Add padding.
    size_t row_buffer_size = (size_t)config->width * 6 + 64;
    char *buffer = malloc(row_buffer_size);
    if (!buffer) {
        perror("Failed to allocate output buffer");
        exit(EXIT_FAILURE);
    }

    if (config->png) {
        // Gnuplot output (Y reversed)
        for (int y = config->height - 1; y >= 0; --y) {
            char *ptr = buffer;
            // Calculate the offset for the start of this row in the flat array
            const int *row_start = &result_buffer[y * config->width];
            for (int x = 0; x < config->width; ++x) {
                int iter = row_start[x];
                if (x > 0) {
                    *ptr++ = ',';
                    *ptr++ = ' ';
                }
                // Manual Integer-to-String (itoa)
                if (iter >= 100) {
                    *ptr++ = '0' + (iter / 100);
                    iter %= 100;
                    *ptr++ = '0' + (iter / 10);
                    *ptr++ = '0' + (iter % 10);
                } else if (iter >= 10) {
                    *ptr++ = '0' + (iter / 10);
                    *ptr++ = '0' + (iter % 10);
                } else {
                    *ptr++ = '0' + iter;
                }
            }
            *ptr++ = '\n';
            fwrite(buffer, 1, ptr - buffer, stdout);
        }
    } else {
        // ASCII output (Standard Y direction)
        for (int y = 0; y < config->height; ++y) {
            char *ptr = buffer;
            const int *row_start = &result_buffer[y * config->width];
            for (int x = 0; x < config->width; ++x) {
                int iter = row_start[x];
                *ptr++ = cnt2char(iter, config->max_iter);
            }
            *ptr++ = '\n';
            fwrite(buffer, 1, ptr - buffer, stdout);
        }
    }
    free(buffer);
}

int main(int argc, char *argv[]) {
    Config config = {
        .width = 100,
        .height = 75,
        .png = false,
        .ll_x = -1.2,
        .ll_y = 0.20,
        .ur_x = -1.0,
        .ur_y = 0.35,
        .max_iter = 255
    };

    for (int i = 1; i < argc; ++i) {
        parse_arg(argv[i], &config);
    }

    size_t total_pixels = config.width * config.height;
    int *result_buffer = malloc(sizeof(int) * total_pixels);
    if (!result_buffer) {
        perror("Failed to allocate result buffer");
        return EXIT_FAILURE;
    }

    pthread_t threads[NUM_THREADS];
    ThreadArgs args[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; ++i) {
        args[i].config = &config;
        args[i].output_buffer = result_buffer;
        pthread_create(&threads[i], NULL, thread_mandelbrot, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    final_output(&config, result_buffer);
    free(result_buffer);
    return EXIT_SUCCESS;
}
