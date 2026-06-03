# Technical Project Guide

## RISC-V Assembly Image Processing using Python, C, QEMU, and WSL2

This document explains how the project works internally, from image loading in Python to assembly execution under QEMU and back to image reconstruction.

## 1. System Goal

The project processes grayscale images using four hand-written RISC-V assembly kernels:

- invert
- threshold
- brightness
- blur

The codebase is split into three major layers:

- Python for orchestration and benchmarking
- C for bridging and RISC-V process control
- RISC-V Assembly for pixel-level processing

## 2. Core Architectural Idea

The project does not attempt to load RISC-V code directly into the Python process.
Instead, Python calls a native bridge library through `ctypes`. That bridge launches a RISC-V executable through `qemu-riscv64`.

This gives the system the following runtime chain:

```text
Python → ctypes → bridge.c → QEMU → RISC-V wrapper → assembly kernels → output.raw → Python
```

This design was chosen because the host Python runtime is x86-64, while the processing executable is RISC-V.

## 3. File Responsibilities

### `python/main.py`

This script handles the full image processing pipeline.

It:

- loads the input grayscale image using OpenCV
- converts the image into a flat `uint8` buffer
- calls the native bridge library through `ctypes`
- saves the processed buffers as PNG images
- prints correctness and timing information

It is the script you run when you want to generate the output images.

### `python/timings.py`

This script compares performance.

It measures:

- pure Python reference implementations
- assembly-backed execution through the bridge

It prints average milliseconds per operation and a speedup ratio.

### `python/pure_python_ref.py`

This file contains the reference Python implementations used for comparison.

It is not part of the RISC-V runtime path.
It exists to provide a baseline for performance comparison.

### `c/bridge.c`

This is the native host-side shared library used by Python.

It performs the following tasks:

- receives requests from Python through `ctypes`
- writes input image data to temporary raw files
- launches `qemu-riscv64`
- invokes the RISC-V executable in either normal or benchmark mode
- captures output
- returns processed results and benchmark values back to Python

This file contains no image processing algorithms.
It is a control and transport layer.

### `c/bridge.h`

This header declares the functions exported by the bridge shared library.

It is used by `bridge.c` and documents the API exposed to Python.

### `c/wrapper.c`

This is the RISC-V executable entry point.

It handles:

- command-line argument parsing
- raw file reading and writing
- memory allocation
- calling the assembly kernels
- benchmark mode execution

The actual image processing logic is not implemented in C.
The C code only orchestrates execution of the assembly functions.

### `c/wrapper.h`

This header declares the assembly-backed image processing functions so the wrapper can call them using C calling conventions.

The functions declared here are implemented in the `.s` files.

### `asm/invert.s`

Performs pixel inversion:

```text
output = 255 - pixel
```

### `asm/threshold.s`

Performs binary thresholding:

```text
pixel >= T ? 255 : 0
```

### `asm/brightness.s`

Performs brightness adjustment with clamping:

```text
pixel = pixel + B
```

with output constrained to the valid `0..255` range.

### `asm/blur.s`

Performs a 3×3 box blur over grayscale image data.

Border pixels are handled separately so the kernel does not read out of bounds.

## 4. Data Flow in Detail

### Full Image Generation Flow

When `python/main.py` is executed:

```text
1. Python loads sample_01.png
2. OpenCV converts it into a grayscale NumPy array
3. NumPy flattens the image into raw bytes
4. Python sends the buffer to bridge.c through ctypes
5. bridge.c writes the buffer to input.raw
6. bridge.c invokes qemu-riscv64
7. QEMU starts the RISC-V executable built from wrapper.c + asm/*.s
8. wrapper.c parses the command line and loads the raw data
9. wrapper.c calls the requested assembly kernel
10. The assembly kernel processes the pixels
11. wrapper.c writes the processed buffer to output.raw
12. QEMU exits
13. bridge.c reads output.raw
14. Python reshapes the buffer back into image form
15. OpenCV writes the final PNG to images/output/
```

## 5. Benchmark Flow

### Python Benchmark

`python/timings.py` first runs the pure Python reference function directly.
That measures the baseline cost of the Python-level algorithm.

### Assembly Benchmark

Then `timings.py` calls a benchmark function exposed by `bridge.c`.
That function:

- writes the input buffer to a temporary raw file
- invokes QEMU
- runs the RISC-V wrapper in benchmark mode
- repeats the kernel many times
- computes an average kernel runtime
- prints `avg_ns=...`

The bridge captures that printed result and returns it to Python.

## 6. Two Different Timing Models

### End-to-End Timing (`main.py`)

This includes:

- Python image load
- temporary raw file creation
- QEMU startup
- RISC-V program startup
- raw file reading/writing
- the assembly kernel itself
- output reconstruction

This is the total user-facing runtime.

### Kernel Timing (`timings.py`)

This focuses on the execution of the assembly kernel inside the RISC-V wrapper benchmark mode.

It is the cleaner number if the goal is to compare algorithm execution rather than full process overhead.

## 7. Why ctypes Is Used

`ctypes` is used so Python can call a native shared library on the host side.

Python itself remains an x86-64 process.
The bridge library is also x86-64.
The bridge then launches the RISC-V executable through QEMU.

This is the only practical way to combine:

- host Python execution
- RISC-V assembly kernels
- QEMU emulation
- benchmarking from Python

without requiring Python itself to run as a RISC-V process.

## 8. Why There Is a RISC-V Wrapper at All

The RISC-V wrapper exists because assembly routines need a C-compatible entry point.

It provides:

- argument parsing
- file handling
- memory allocation
- assembly function invocation
- benchmark mode

Without the wrapper, the assembly files would not be runnable as a standalone program.

## 9. Why the C Files Do Not Implement Image Processing

Neither `bridge.c` nor `wrapper.c` contains the image processing algorithms.

They only manage:

- data movement
- process control
- timing capture
- file I/O
- function dispatch

The actual arithmetic and pixel manipulation live in the assembly kernels.

## 10. Build Outputs

Two build artifacts are expected:

- `build/libbridge.so` — the native bridge library loaded by Python
- `build/app` — the RISC-V executable run under QEMU

Typical build commands:

```bash
gcc -O2 -fPIC -shared -Wall -Wextra c/bridge.c -o build/libbridge.so
riscv64-linux-gnu-gcc -march=rv64imafd -mabi=lp64d -mno-relax c/wrapper.c asm/*.s -o build/app
```

## 11. Execution Summary

### Image generation mode

```text
python/main.py
→ ctypes
→ bridge.c
→ QEMU
→ wrapper.c
→ assembly
→ output PNG
```

### Benchmark mode

```text
python/timings.py
→ ctypes
→ bridge.c
→ QEMU
→ wrapper.c benchmark mode
→ assembly repeated many times
→ avg_ns returned to Python
```

## 12. Practical Meaning of the Results

The benchmark results should be interpreted carefully:

- For simple operations like invert, threshold, and brightness, NumPy may outperform emulated RISC-V because NumPy uses highly optimized native code.
- For more loop-heavy workloads like blur, the hand-written assembly may outperform the pure Python implementation by a wide margin.

This does not mean assembly is always faster in general.
It means the performance depends on:

- how optimized the reference implementation is
- how expensive the algorithm is
- how much overhead QEMU and process launching add

## 13. What the Project Demonstrates

This project demonstrates:

- low-level image processing in assembly
- C calling conventions
- RISC-V executable construction
- cross-architecture execution with QEMU
- Python-to-native interoperability through ctypes
- benchmark design and interpretation
- the difference between kernel time and end-to-end runtime

## 14. Summary

The project is built around a layered architecture:

- Python orchestrates
- bridge.c connects Python to the RISC-V side
- QEMU executes the RISC-V binary
- wrapper.c coordinates execution on the RISC-V side
- assembly performs the actual pixel transformations

This keeps the system modular, testable, and easy to benchmark while preserving the low-level assembly focus required by the course.