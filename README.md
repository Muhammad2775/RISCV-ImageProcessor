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
Python communicates with a native shared library through `ctypes`. The bridge library is responsible for calling the RISC-V executable through QEMU and returning processed data to Python.

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

- Receives requests from Python
- Creates temporary raw input/output files
- Launches `qemu-riscv64`
- Runs the RISC-V executable in normal or benchmark mode
- Reads back results and timing values

### RISC-V Wrapper (`wrapper.c`)
The wrapper is the executable entry point for the RISC-V side. It:

- Parses command-line arguments
- Reads raw image buffers
- Allocates output buffers
- Calls the assembly routines
- Writes the processed raw output
- Provides a benchmark mode for kernel timing

### Assembly Kernels
The assembly files contain the actual image processing logic:

- `invert.s` — pixel inversion
- `threshold.s` — binary thresholding
- `brightness.s` — brightness adjustment with clamping
- `blur.s` — 3×3 box blur

## Data Processing Pipeline

The pipeline is deterministic and file-buffer driven:

```text
Python image load → raw buffer → ctypes bridge → QEMU → RISC-V wrapper → assembly kernel → output.raw → Python image reconstruction
```

This approach keeps the architecture boundaries explicit and makes debugging easier.

## Performance Profile

The project supports two performance views:

- **End-to-end timing**: measures the full Python → bridge → QEMU → RISC-V path
- **Kernel timing**: measures only the assembly kernel execution inside the RISC-V wrapper benchmark mode

These timings are useful for comparing:

- pure Python implementations
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
│   └── pure_python_ref.py
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

This repository demonstrates how Python, C, QEMU, and RISC-V Assembly can be combined into a clean low-level image processing pipeline. It is designed as a practical systems project that emphasizes architecture clarity, benchmarking, and correct cross-language execution.
