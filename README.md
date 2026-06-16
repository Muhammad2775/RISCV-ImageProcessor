# RISC-V Assembly Image Processing
**RISC-V Assembly Image Processing** is a systems-oriented image processing project that combines **Python**, **C**, **ctypes**, **QEMU**, **WSL2**, and **RISC-V Assembly** into a single end-to-end workflow. It loads grayscale images, applies low-level image filters, benchmarks the results, and saves the processed outputs as image files.

## Overview
This project implements four image processing operations in **RISC-V Assembly**:

- Image inversion
- Thresholding
- Brightness adjustment
- 3×3 box blur

Python acts as the orchestration layer, while a native C bridge launches the RISC-V executable through QEMU and returns results back to Python. The project is structured to demonstrate low-level programming, cross-architecture execution, and performance analysis.

## Design Principles

### Native Python Frontend
Python is responsible for image loading, output reconstruction, and benchmarking. It uses OpenCV and NumPy to handle grayscale images as raw buffers.

### ctypes-Based Bridge
Python communicates with a native shared library through ctypes. The bridge launches the RISC-V executable through QEMU and performs stream-based inter-process communication (IPC) using standard input and output to exchange image data and benchmark results.

### RISC-V Execution Model
The actual image processing happens in a RISC-V executable that calls hand-written assembly routines. This keeps the core computation at the assembly level while preserving a clean interface to Python.

### Benchmark-Oriented Architecture
The project includes both full-pipeline timings and kernel-level timings so that Python and assembly performance can be compared clearly.

## Core Architecture

### Python Layer
The Python layer performs the following tasks:

- Loads the grayscale input image
- Converts the image into a flat byte buffer
- Invokes the native bridge through `ctypes`
- Reconstructs output buffers into images
- Saves processed images to the output folder
- Runs performance comparisons against pure Python implementations

### Bridge Layer (`bridge.c`)
The bridge layer is a native host-side shared library. It:

Receives requests from Python through ctypes
Creates UNIX pipes for communication with the child process
Launches qemu-riscv64 and executes the RISC-V application
Streams image data to the RISC-V wrapper through standard input
Receives processed image buffers or benchmark timing results through standard output
Converts native errors into messages accessible from Python

### RISC-V Wrapper (`wrapper.c`)
The wrapper is the RISC-V executable entry point. It dispatches requested image operations, reads input image streams from standard input, invokes the appropriate assembly kernel, writes processed output to standard output, and provides a benchmark mode that repeatedly executes assembly kernels to measure average execution time.

### Assembly Kernels
The assembly files contain the actual image processing logic:

- `invert.s` — pixel inversion
- `threshold.s` — binary thresholding
- `brightness.s` — brightness adjustment with clamping
- `blur.s` — 3×3 box blur

## Data Processing Pipeline
The pipeline is deterministic and file-buffer driven:

```
Python image
      |
      v
NumPy byte buffer
      |
      v
ctypes interface
      |
      v
C bridge (libbridge.so)
      |
      v
stdin/stdout pipes
      |
      v
QEMU RISC-V emulator
      |
      v
RISC-V wrapper
      |
      v
Assembly image kernels
      |
      v
Processed byte stream
      |
      v
Python image reconstruction
```

This approach keeps the architecture boundaries explicit and makes debugging easier.

## Performance Profile
The project supports two performance views:

- **End-to-end timing**: measures the full Python → bridge → QEMU → RISC-V path
- **Kernel timing**: measures only the assembly kernel execution inside the RISC-V wrapper benchmark mode

These timings are useful for comparing:

- pure python implementations
- NumPy-based reference behavior
- RISC-V assembly execution
- emulation overhead under QEMU

## Project Structure
```text
COAL-Semester-Project/
├── asm/
│   ├── invert.s
│   ├── threshold.s
│   ├── brightness.s
│   └── blur.s
├── c/
│   ├── wrapper.c
│   ├── wrapper.h
│   ├── bridge.c
│   └── bridge.h
├── python/
│   ├── main.py
│   ├── timings.py
│   └── python_implementation.py
├── images/
│   ├── input/
│   │   └── sample_01.png
│   └── output/
├── build/
└── README.md
```

## Typical Usage
### Generate output images

```bash
python3 python/main.py
```

### Run benchmark comparison

```bash
python3 python/timings.py
```

## Current Scope
This project focuses on offline grayscale image processing and performance comparison. It does not include:

- color image support
- real-time video processing
- distributed execution
- GPU acceleration
- online inference or detection pipelines

## Summary
This repository demonstrates how Python, C, ctypes, QEMU, and RISC-V Assembly can be integrated into a clean low-level image processing pipeline. It uses a stream-oriented inter-process communication (IPC) architecture to enable efficient data exchange between the host environment and the emulated RISC-V application. The project is designed as a practical systems programming exercise that emphasizes cross-language integration, architecture clarity, performance benchmarking, and low-level software design.