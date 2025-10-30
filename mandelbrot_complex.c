/**
 * @file mandelbrot_complex.c
 * @brief Generates Mandelbrot set visualizations using C23 complex types.
 *
 * A modern C implementation for a cross-language comparison project.
 * It parses command-line arguments in the format key=value.
 * This version uses the <complex.h> header for more expressive math.
 *
 * Compilation:
 * gcc -std=c23 -o mandelbrot mandelbrot_complex.c -lm -O3
 *
 * Usage:
 * ./mandelbrot
 * ./mandelbrot width=120 ll_x=-0.75 ll_y=0.1 ur_x=-0.74 ur_y=0.11
 * ./mandelbrot png=1 width=800 height=600 > mandelbrot.dat
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <complex.h>

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
 * @param c The complex number to test.
 * @param max_iter The maximum number of iterations.
 * @return An integer representing how close the point is to the set.
 */
int escape_time(double complex c, int max_iter) {
    double complex z = 0.0 + 0.0 * I; // Start with z = 0
    int iter;

    for (iter = 0; iter < max_iter; ++iter) {
        // The magnitude squared is creal(z)*creal(z) + cimag(z)*cimag(z)
        // This is more efficient than cabs(z) > 2.0, as it avoids a sqrt()
        if (creal(z) * creal(z) + cimag(z) * cimag(z) > 4.0) {
            break;
        }
        z = z * z + c;
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
            double complex c = real + imag * I;

            int iter = escape_time(c, config->max_iter);
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
            double complex c = real + imag * I;

            int iter = escape_time(c, config->max_iter);
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

    if (config.png) {
        gptext_output(&config);
    } else {
        ascii_output(&config);
    }

    return EXIT_SUCCESS;
}
