#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "wrapper.h"

#define SYSROOT "/usr/riscv64-linux-gnu"

static void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s invert <input.raw> <output.raw> <size>\n"
        "  %s threshold <input.raw> <output.raw> <size> <T>\n"
        "  %s brightness <input.raw> <output.raw> <size> <B>\n"
        "  %s blur <input.raw> <output.raw> <width> <height>\n"
        "  %s bench invert <input.raw> <size> <iterations>\n"
        "  %s bench threshold <input.raw> <size> <T> <iterations>\n"
        "  %s bench brightness <input.raw> <size> <B> <iterations>\n"
        "  %s bench blur <input.raw> <width> <height> <iterations>\n",
        prog, prog, prog, prog, prog, prog, prog, prog);
}

static uint64_t now_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        die("clock_gettime failed.");
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static uint8_t* xmalloc_bytes(size_t n) {
    uint8_t* p = (uint8_t*)malloc(n);
    if (!p) {
        die("Memory allocation failed.");
    }
    return p;
}

static uint8_t* read_raw_alloc(const char* path, size_t size) {
    FILE* f = fopen(path, "rb");
    if (!f) die("Failed to open input file.");

    uint8_t* buf = xmalloc_bytes(size);

    if (fread(buf, 1, size, f) != size) {
        fclose(f);
        free(buf);
        die("Failed to read input data.");
    }

    fclose(f);
    return buf;
}

static void write_raw(const char* path, const uint8_t* data, size_t size) {
    FILE* f = fopen(path, "wb");
    if (!f) die("Failed to open output file.");

    if (fwrite(data, 1, size, f) != size) {
        fclose(f);
        die("Failed to write output data.");
    }

    fclose(f);
}

static double bench_invert(const uint8_t* input, int size, int iterations) {
    uint8_t* output = xmalloc_bytes((size_t)size);

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

static double bench_threshold(const uint8_t* input, int size, uint8_t T, int iterations) {
    uint8_t* output = xmalloc_bytes((size_t)size);

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

static double bench_brightness(const uint8_t* input, int size, int B, int iterations) {
    uint8_t* output = xmalloc_bytes((size_t)size);

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

static double bench_blur(const uint8_t* input, int width, int height, int iterations) {
    int size = width * height
	//Even though there a narrow type conversion occurs, it seems like C is less strict in these things compared to C++ though I might need to confirm this
    uint8_t* output = xmalloc_bytes((size_t)size);

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

static int run_invert(const char* input_path, const char* output_path, int size) {
    uint8_t* input = read_raw_alloc(input_path, (size_t)size);
    uint8_t* output = xmalloc_bytes((size_t)size);

    invert(input, output, size);
    write_raw(output_path, output, (size_t)size);

    free(input);
    free(output);
    return 0;
}

static int run_threshold(const char* input_path, const char* output_path, int size, uint8_t T) {
    uint8_t* input = read_raw_alloc(input_path, (size_t)size);
    uint8_t* output = xmalloc_bytes((size_t)size);

    threshold(input, output, size, T);
    write_raw(output_path, output, (size_t)size);

    free(input);
    free(output);
    return 0;
}

static int run_brightness(const char* input_path, const char* output_path, int size, int B) {
    uint8_t* input = read_raw_alloc(input_path, (size_t)size);
    uint8_t* output = xmalloc_bytes((size_t)size);

    brightness(input, output, size, B);
    write_raw(output_path, output, (size_t)size);

    free(input);
    free(output);
    return 0;
}

static int run_blur(const char* input_path, const char* output_path, int width, int height) {
    int size = width * height;
    uint8_t* input = read_raw_alloc(input_path, (size_t)size);
    uint8_t* output = xmalloc_bytes((size_t)size);

    blur(input, output, width, height);
    write_raw(output_path, output, (size_t)size);

    free(input);
    free(output);
    return 0;
}

/*	C++ auto handles the cli args but even though this is C11 it doesnt do that. Besides that there are numerous stupid pointer and representation nuances in Unix 
	systems because for some reason this hasn't changed since C99 even in C23. But since the program name is implementation defined in the standard so an 
	implementation is free to do what it wants, including allowing something in there that isn't the actual name and considring this is more of a 
	standards and ABI issue, its better to leave it alone if it works.
	
	Note: "However, implementation-defined does have a specific meaning in the ISO standards as the implementation must document how it works. So even UNIX, which 
	can put anything it likes into argv[0] with the exec family of calls, has to (and does) document it, since argv[0] can hold such diverse things such as the full 
	path of the program ('/progpath/prog'), just the filename ('prog'), a slightly modified name ('-prog'), a descriptive name ('prog - a program for progging') 
	and nothing (''). The implementation has to define what it holds but that's all the standard requires. However, these have never been issues in Windows."
	
	READ: "https://stackoverflow.com/questions/2050961/is-argv0-name-of-executable-an-accepted-standard-or-just-a-common-conventi/2051031#2051031"
*/

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "bench") == 0) {
        if (argc < 6) {
            usage(argv[0]);
            return 1;
        }

        const char* op = argv[2];
        const char* input_path = argv[3];

        if (strcmp(op, "invert") == 0) {
            if (argc != 6) { usage(argv[0]); return 1; }
            int size = atoi(argv[4]);
            int iterations = atoi(argv[5]);
            if (size <= 0 || iterations <= 0) die("Invalid size/iterations.");
            uint8_t* input = read_raw_alloc(input_path, (size_t)size);
            double avg_ns = bench_invert(input, size, iterations);
            printf("avg_ns=%.2f\n", avg_ns);
            free(input);
            return 0;
        }

        if (strcmp(op, "threshold") == 0) {
            if (argc != 7) { usage(argv[0]); return 1; }
            int size = atoi(argv[4]);
            uint8_t T = (uint8_t)atoi(argv[5]);
            int iterations = atoi(argv[6]);
            if (size <= 0 || iterations <= 0) die("Invalid size/iterations.");
            uint8_t* input = read_raw_alloc(input_path, (size_t)size);
            double avg_ns = bench_threshold(input, size, T, iterations);
            printf("avg_ns=%.2f\n", avg_ns);
            free(input);
            return 0;
        }

        if (strcmp(op, "brightness") == 0) {
            if (argc != 7) { usage(argv[0]); return 1; }
            int size = atoi(argv[4]);
            int B = atoi(argv[5]);
            int iterations = atoi(argv[6]);
            if (size <= 0 || iterations <= 0) die("Invalid size/iterations.");
            uint8_t* input = read_raw_alloc(input_path, (size_t)size);
            double avg_ns = bench_brightness(input, size, B, iterations);
            printf("avg_ns=%.2f\n", avg_ns);
            free(input);
            return 0;
        }

        if (strcmp(op, "blur") == 0) {
            if (argc != 7) { usage(argv[0]); return 1; }
            int width = atoi(argv[4]);
            int height = atoi(argv[5]);
            int iterations = atoi(argv[6]);
            if (width <= 0 || height <= 0 || iterations <= 0) die("Invalid width/height/iterations.");
            uint8_t* input = read_raw_alloc(input_path, (size_t)(width * height));
            double avg_ns = bench_blur(input, width, height, iterations);
            printf("avg_ns=%.2f\n", avg_ns);
            free(input);
            return 0;
        }

        usage(argv[0]);
        return 1;
    }

    const char* op = argv[1];
    if (strcmp(op, "invert") == 0) {
        if (argc != 5) { usage(argv[0]); return 1; }
        return run_invert(argv[2], argv[3], atoi(argv[4]));
    }

    if (strcmp(op, "threshold") == 0) {
        if (argc != 6) { usage(argv[0]); return 1; }
        return run_threshold(argv[2], argv[3], atoi(argv[4]), (uint8_t)atoi(argv[5]));
    }

    if (strcmp(op, "brightness") == 0) {
        if (argc != 6) { usage(argv[0]); return 1; }
        return run_brightness(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]));
    }

    if (strcmp(op, "blur") == 0) {
        if (argc != 6) { usage(argv[0]); return 1; }
        return run_blur(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]));
    }

    usage(argv[0]);
    return 1;
}