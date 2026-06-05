#ifndef BRIDGE_H
#define BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void bridge_set_qemu_path(const char *path);
void bridge_set_app_path(const char *path);
const char *bridge_last_error(void);

int bridge_invert(const uint8_t *input, uint8_t *output, int size);
int bridge_threshold(const uint8_t *input, uint8_t *output, int size, uint8_t T);
int bridge_brightness(const uint8_t *input, uint8_t *output, int size, int B);
int bridge_blur(const uint8_t *input, uint8_t *output, int width, int height);

double bridge_bench_invert(const uint8_t *input, int size, int iterations);
double bridge_bench_threshold(const uint8_t *input, int size, uint8_t T, int iterations);
double bridge_bench_brightness(const uint8_t *input, int size, int B, int iterations);
double bridge_bench_blur(const uint8_t *input, int width, int height, int iterations);

#ifdef __cplusplus
}
#endif

#endif