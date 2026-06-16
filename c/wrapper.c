#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "wrapper.h"

static const char* prog = "app";

static void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void usage(void) {
    fprintf(stderr,
        "Usage:\n"
        "  %s invert <size>\n"
        "  %s threshold <size> <T>\n"
        "  %s brightness <size> <B>\n"
        "  %s blur <width> <height>\n"
        "  %s bench invert <size> <iterations>\n"
        "  %s bench threshold <size> <T> <iterations>\n"
        "  %s bench brightness <size> <B> <iterations>\n"
        "  %s bench blur <width> <height> <iterations>\n",
        prog, prog, prog, prog, prog, prog, prog, prog);
}

static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        die("clock_gettime failed.");
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int parse_pos_int(const char* s, const char* name) {
    if (!s) {
        fprintf(stderr, "Missing argument: %s\n", name);
        exit(1);
    }

    errno = 0;
    char* end = NULL;
    long v = strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0' || v <= 0 || v > INT_MAX) {
        fprintf(stderr, "Invalid %s: %s\n", name, s);
        exit(1);
    }

    return (int)v;
}

static int parse_int_any(const char* s, const char* name) {
    if (!s) {
        fprintf(stderr, "Missing argument: %s\n", name);
        exit(1);
    }

    errno = 0;
    char* end = NULL;
    long v = strtol(s, &end, 10);

    if (errno != 0 || end == s || *end != '\0' || v < INT_MIN || v > INT_MAX) {
        fprintf(stderr, "Invalid %s: %s\n", name, s);
        exit(1);
    }

    return (int)v;
}

static void read_exact_stdin(uint8_t* buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = read(STDIN_FILENO, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            die("Failed to read input bytes from stdin.");
        }
        if (n == 0) {
            die("Unexpected EOF on stdin.");
        }
        off += (size_t)n;
    }
}

static void write_exact_stdout(const uint8_t* buf, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(STDOUT_FILENO, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            die("Failed to write output bytes to stdout.");
        }
        off += (size_t)n;
    }
}

static double bench_invert_kernel(const uint8_t* input, int size, int iterations) {
    uint8_t* output = (uint8_t*)malloc((size_t)size);
    if (!output) die("Memory allocation failed.");

    uint64_t start = now_ns();
    for (int i = 0; i < iterations; ++i) {
        invert((uint8_t*)input, output, size);
    }
    uint64_t end = now_ns();

    volatile uint8_t sink = output[0];
    (void)sink;

    free(output);
    return (double)(end - start) / (double)iterations;
}

static double bench_threshold_kernel(const uint8_t* input, int size, uint8_t T, int iterations) {
    uint8_t* output = (uint8_t*)malloc((size_t)size);
    if (!output) die("Memory allocation failed.");

    uint64_t start = now_ns();
    for (int i = 0; i < iterations; ++i) {
        threshold((uint8_t*)input, output, size, T);
    }
    uint64_t end = now_ns();

    volatile uint8_t sink = output[0];
    (void)sink;

    free(output);
    return (double)(end - start) / (double)iterations;
}

static double bench_brightness_kernel(const uint8_t* input, int size, int B, int iterations) {
    uint8_t* output = (uint8_t*)malloc((size_t)size);
    if (!output) die("Memory allocation failed.");

    uint64_t start = now_ns();
    for (int i = 0; i < iterations; ++i) {
        brightness((uint8_t*)input, output, size, B);
    }
    uint64_t end = now_ns();

    volatile uint8_t sink = output[0];
    (void)sink;

    free(output);
    return (double)(end - start) / (double)iterations;
}

static double bench_blur_kernel(const uint8_t* input, int width, int height, int iterations) {
    int size = width * height;
    uint8_t* output = (uint8_t*)malloc((size_t)size);
    if (!output) die("Memory allocation failed.");

    uint64_t start = now_ns();
    for (int i = 0; i < iterations; ++i) {
        blur((uint8_t*)input, output, width, height);
    }
    uint64_t end = now_ns();

    volatile uint8_t sink = output[0];
    (void)sink;

    free(output);
    return (double)(end - start) / (double)iterations;
}

static int run_invert(const char* const* args, int argc) {
    if (argc != 1) { usage(); return 1; }

    int size = parse_pos_int(args[0], "size");
    uint8_t* input = (uint8_t*)malloc((size_t)size);
    uint8_t* output = (uint8_t*)malloc((size_t)size);
    if (!input || !output) die("Memory allocation failed.");

    read_exact_stdin(input, (size_t)size);
    invert(input, output, size);
    write_exact_stdout(output, (size_t)size);

    free(input);
    free(output);
    return 0;
}

static int run_threshold(const char* const* args, int argc) {
    if (argc != 2) { usage(); return 1; }

    int size = parse_pos_int(args[0], "size");
    int T = parse_int_any(args[1], "T");

    uint8_t* input = (uint8_t*)malloc((size_t)size);
    uint8_t* output = (uint8_t*)malloc((size_t)size);
    if (!input || !output) die("Memory allocation failed.");

    read_exact_stdin(input, (size_t)size);
    threshold(input, output, size, (uint8_t)T);
    write_exact_stdout(output, (size_t)size);

    free(input);
    free(output);
    return 0;
}

static int run_brightness(const char* const* args, int argc) {
    if (argc != 2) { usage(); return 1; }

    int size = parse_pos_int(args[0], "size");
    int B = parse_int_any(args[1], "B");

    uint8_t* input = (uint8_t*)malloc((size_t)size);
    uint8_t* output = (uint8_t*)malloc((size_t)size);
    if (!input || !output) die("Memory allocation failed.");

    read_exact_stdin(input, (size_t)size);
    brightness(input, output, size, B);
    write_exact_stdout(output, (size_t)size);

    free(input);
    free(output);
    return 0;
}

static int run_blur(const char* const* args, int argc) {
    if (argc != 2) { usage(); return 1; }

    int width = parse_pos_int(args[0], "width");
    int height = parse_pos_int(args[1], "height");
    int size = width * height;

    uint8_t* input = (uint8_t*)malloc((size_t)size);
    uint8_t* output = (uint8_t*)malloc((size_t)size);
    if (!input || !output) die("Memory allocation failed.");

    read_exact_stdin(input, (size_t)size);
    blur(input, output, width, height);
    write_exact_stdout(output, (size_t)size);

    free(input);
    free(output);
    return 0;
}

static int bench_invert(const char* const* args, int argc) {
    if (argc != 2) { usage(); return 1; }

    int size = parse_pos_int(args[0], "size");
    int iterations = parse_pos_int(args[1], "iterations");
    uint8_t* input = (uint8_t*)malloc((size_t)size);
    if (!input) die("Memory allocation failed.");

    read_exact_stdin(input, (size_t)size);
    double avg_ns = bench_invert_kernel(input, size, iterations);
    printf("avg_ns=%.2f\n", avg_ns);
    fflush(stdout);

    free(input);
    return 0;
}

static int bench_threshold(const char* const* args, int argc) {
    if (argc != 3) { usage(); return 1; }

    int size = parse_pos_int(args[0], "size");
    int T = parse_int_any(args[1], "T");
    int iterations = parse_pos_int(args[2], "iterations");

    uint8_t* input = (uint8_t*)malloc((size_t)size);
    if (!input) die("Memory allocation failed.");

    read_exact_stdin(input, (size_t)size);
    double avg_ns = bench_threshold_kernel(input, size, (uint8_t)T, iterations);
    printf("avg_ns=%.2f\n", avg_ns);
    fflush(stdout);

    free(input);
    return 0;
}

static int bench_brightness(const char* const* args, int argc) {
    if (argc != 3) { usage(); return 1; }

    int size = parse_pos_int(args[0], "size");
    int B = parse_int_any(args[1], "B");
    int iterations = parse_pos_int(args[2], "iterations");

    uint8_t* input = (uint8_t*)malloc((size_t)size);
    if (!input) die("Memory allocation failed.");

    read_exact_stdin(input, (size_t)size);
    double avg_ns = bench_brightness_kernel(input, size, B, iterations);
    printf("avg_ns=%.2f\n", avg_ns);
    fflush(stdout);

    free(input);
    return 0;
}

static int bench_blur(const char* const* args, int argc) {
    if (argc != 3) { usage(); return 1; }

    int width = parse_pos_int(args[0], "width");
    int height = parse_pos_int(args[1], "height");
    int iterations = parse_pos_int(args[2], "iterations");

    size_t size = (size_t)width * (size_t)height;
    uint8_t* input = (uint8_t*)malloc(size);
    if (!input) die("Memory allocation failed.");

    read_exact_stdin(input, size);
    double avg_ns = bench_blur_kernel(input, width, height, iterations);
    printf("avg_ns=%.2f\n", avg_ns);
    fflush(stdout);

    free(input);
    return 0;
}

typedef struct {
    const char* name;
    int (*run)(const char* const* args, int argc);
    int (*bench)(const char* const* args, int argc);
} Command;

static const Command COMMANDS[] = {
    {"invert", run_invert, bench_invert},
    {"threshold", run_threshold, bench_threshold},
    {"brightness", run_brightness, bench_brightness},
    {"blur", run_blur, bench_blur},
};

static const Command* find_command(const char* name) {
    for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); ++i) {
        if (strcmp(COMMANDS[i].name, name) == 0) {
            return &COMMANDS[i];
        }
    }
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 1;
    }

    prog = argv[0];

    if (strcmp(argv[1], "bench") == 0) {
        if (argc < 4) {
            usage();
            return 1;
        }

        const Command* cmd = find_command(argv[2]);
        if (!cmd) {
            usage();
            return 1;
        }

        return cmd->bench((const char* const*)&argv[3], argc - 3);
    }

    const Command* cmd = find_command(argv[1]);
    if (!cmd) {
        usage();
        return 1;
    }

    return cmd->run((const char* const*)&argv[2], argc - 2);
}