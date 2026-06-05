#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
    extern "C" {
#endif

    void invert(uint8_t* input, uint8_t* output, int size);
    void threshold(uint8_t* input, uint8_t* output, int size, uint8_t T);
    void brightness(uint8_t* input, uint8_t* output, int size, int B);
    void blur(uint8_t* input, uint8_t* output, int width, int height);

    #ifdef __cplusplus
}
#endif

#endif