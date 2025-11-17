/**
 * @file mandelbrot_pthread.c
 * @brief Generates Mandelbrot set visualizations in ASCII or for gnuplot.
 *
 * A modern C implementation for a cross-language comparison project.
 * It parses command-line arguments in the format key=value.
 *
 * Compilation:
 * gcc -o mandelbrot mandelbrot_pthread.c -lm -O3 -pthread
 * clang -o mandelbrot mandelbrot_pthread.c -lm -O3 -pthread
 *
 * Usage:
 * ./mandelbrot
 * ./mandelbrot width=120 ll_x=-0.75 ll_y=0.1 ur_x=-0.74 ur_y=0.11
 * ./mandelbrot png=1 width=800 height=600 > mandelbrot.dat
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define NUM_THREADS 9 // Adjust based on CPU core count
#define CHUNK_SIZE  1  // Number of rows to process per task

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
 * @brief Renders the Mandelbrot set as ASCII art to stdout.
 * @param config A pointer to the configuration struct.
 */
void ascii_output(const Config *config) {
    double fwidth = config->ur_x - config->ll_x;
    double fheight = config->ur_y - config->ll_y;

    for (int y = 0; y < config->height; ++y) {
        for (int x = 0; x < config->width; ++x) {
            double real = config->ll_x + x * fwidth / config->width;
            double imag = config->ur_y - y * fheight / config->height;
            int iter = escape_time(real, imag, config->max_iter);
            putchar(cnt2char(iter, config->max_iter));
        }
        putchar('\n');
    }
}

/**
 * @brief Generates text output suitable for gnuplot to stdout.
 * @param config A pointer to the configuration struct.
 */
void gptext_output(const Config *config) {
    double fwidth = config->ur_x - config->ll_x;
    double fheight = config->ur_y - config->ll_y;

    for (int y = config->height; y > 0; --y) {
        for (int x = 0; x < config->width; ++x) {
            double real = config->ll_x + x * fwidth / config->width;
            double imag = config->ur_y - y * fheight / config->height;
            int iter = escape_time(real, imag, config->max_iter);
            // Print comma separator for all but the first value in a row
            printf("%s%d", (x > 0 ? ", " : ""), iter);
        }
        printf("\n");
    }
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
            for (int x = 0; x < config->width; ++x) {
                double real = config->ll_x + x * fwidth / config->width;
                double imag = config->ur_y - y * fheight / config->height;

                int iter = escape_time(real, imag, config->max_iter);
                buffer[y * config->width + x] = iter;
            }
        }
    }
    return NULL;
}

void final_output(const Config *config, const int *result_buffer) {
    if (config->png) {
        // Output for gnuplot 
        for (int y = config->height - 1; y >= 0; --y) { // Loop reversed for gnuplot
            for (int x = 0; x < config->width; ++x) {
                printf("%s%d", (x > 0 ? ", " : ""), result_buffer[y * config->width + x]);
            }
            printf("\n");
        }
    } else {
        // ASCII output 
        for (int y = 0; y < config->height; ++y) {
            for (int x = 0; x < config->width; ++x) {
                int iter = result_buffer[y * config->width + x];
                putchar(cnt2char(iter, config->max_iter));
            }
            printf("\n");
        }
    }
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
