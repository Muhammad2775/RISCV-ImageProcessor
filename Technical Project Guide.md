# Technical Project Guide

# RISC-V Assembly Image Processing using Python, C, ctypes, QEMU, and WSL2

## 1. Introduction
This document provides a complete technical explanation of the internal design, architecture, execution flow, and performance model of the RISC-V Assembly Image Processing project.

The project demonstrates how high-level software written in Python can interact with low-level RISC-V assembly routines through a native C interoperability layer. Because the host machine executes an x86-64 operating system while the image processing algorithms are compiled for the RISC-V instruction set architecture (ISA), the system uses QEMU user-mode emulation to execute RISC-V binaries transparently on the host environment.

The project combines multiple software layers:

* Python for application orchestration, image handling, validation, and benchmarking.
* C for native interoperability, process management, and stream-oriented inter-process communication (IPC).
* RISC-V C wrapper code for execution control and assembly integration.
* RISC-V Assembly for the actual pixel-level image processing algorithms.
* QEMU for cross-architecture execution between the x86-64 host and the RISC-V application.

The objective is not only to implement image filters but also to demonstrate important systems programming concepts such as:

* Foreign Function Interfaces (FFI).
* Cross-language communication.
* Cross-architecture execution.
* Process creation and control.
* Stream-based IPC using UNIX pipes.
* Low-level memory management.
* Assembly integration with C.
* Performance measurement and benchmarking.

# 2. System Goal
The purpose of the project is to process grayscale images using hand-written RISC-V assembly kernels.

The implemented image processing operations are:

* Image inversion.
* Binary thresholding.
* Brightness adjustment with saturation.
* 3×3 box blur filtering.

Each operation is implemented directly in RISC-V assembly and follows the RISC-V calling convention so that it can be invoked from C.

The architecture intentionally separates the responsibilities of different languages:

* Python provides usability, automation, and benchmarking.
* C provides a controlled bridge between Python and the RISC-V execution environment.
* Assembly provides direct low-level manipulation of image data.

This layered approach makes the system modular, easier to test, and easier to extend.

# 3. Overall System Architecture
The application is built around a stream-oriented inter-process communication architecture.

The complete execution chain is the folowing:

```text
Python Application
        |
        v
ctypes Foreign Function Interface
        |
        v
Native C Bridge (libbridge.so)
        |
        v
UNIX stdin/stdout Pipes
        |
        v
QEMU RISC-V Emulator
        |
        v
RISC-V Executable (wrapper.c)
        |
        v
RISC-V Assembly Kernels
        |
        v
Processed Byte Stream
        |
        v
Python Image Reconstruction
```

The Python process and the RISC-V executable never directly share memory.

Instead, image buffers are transferred as streams of raw bytes through operating-system pipes.

This design provides clear isolation between the host environment and the emulated RISC-V environment while avoiding the overhead and complexity of temporary intermediate files.

# 4. Why ctypes Is Required
Python cannot directly execute RISC-V assembly functions because:

* Python executes as an x86-64 process.
* RISC-V instructions are incompatible with the host CPU architecture.
* The RISC-V code exists as a separate executable rather than a native Python module.

To solve this problem, Python uses the ctypes Foreign Function Interface (FFI) to communicate with a native x86-64 shared library.

The bridge library exposes a C API that Python can call directly.

Examples of exposed functions include:

* bridge_invert()
* bridge_threshold()
* bridge_brightness()
* bridge_blur()

and their corresponding benchmark variants.

The bridge hides all details related to:

* Creating child processes.
* Starting QEMU.
* Passing command-line arguments.
* Transferring image data.
* Receiving results.
* Handling execution errors.

From the perspective of Python, a RISC-V assembly operation appears like a normal function call.

# 5. Python Layer
The Python layer consists of three primary files.

## 5.1 main.py
main.py is responsible for correctness testing and full pipeline execution.

Its responsibilities include:

* Loading input grayscale images using OpenCV.
* Storing image data as NumPy uint8 arrays.
* Preparing contiguous memory buffers suitable for ctypes.
* Calling the C bridge functions.
* Receiving processed image buffers.
* Comparing assembly output against Python reference implementations.
* Measuring complete execution time.
* Saving generated output images.

The timing measured in main.py represents the complete user-visible runtime.

It includes:

* Python execution overhead.
* ctypes function calls.
* C bridge execution.
* Pipe communication.
* QEMU startup.
* RISC-V program execution.
* Assembly kernel execution.
* Data transfer back to Python.
* Image reconstruction and file output.

## 5.2 timings.py
timings.py is responsible for detailed performance comparison.

Unlike main.py, which measures the entire execution pipeline, timings.py focuses on comparing algorithm execution performance.

It measures:

### Python performance
The reference Python implementations are executed repeatedly.

Average milliseconds per operation are calculated.

### RISC-V assembly performance
The benchmark functions exposed by the bridge are called.

The RISC-V wrapper repeatedly executes the assembly kernels internally and calculates the average kernel execution time in nanoseconds.

This approach minimizes external overhead and provides a more meaningful comparison of algorithm performance.

The results include:

* Python average execution time.
* Assembly average execution time.
* Relative speedup or slowdown.

## 5.3 python_implementation.py
This file contains reference implementations of every image processing algorithm using standard Python loops.

It includes:

* invert()
* threshold()
* brightness()
* blur()

These implementations serve two purposes:

1. Correctness validation by comparing their output against the assembly implementation.
2. A baseline for performance comparisons.

They are intentionally implemented using explicit loops instead of relying on highly optimized NumPy vectorized operations.

This allows the comparison to focus on algorithm implementation rather than optimized library routines.

# 6. C Bridge Layer (bridge.c)
bridge.c is a native x86-64 shared library loaded by Python.

It acts as the communication boundary between the host Python process and the RISC-V execution environment.

Its responsibilities include:

* Receiving requests from Python.
* Creating UNIX pipes for input and output streams.
* Launching child processes using fork().
* Executing QEMU using execvp().
* Passing the RISC-V executable path and arguments.
* Streaming raw image bytes into the child process through stdin.
* Receiving processed image bytes or benchmark output through stdout.
* Capturing and reporting execution failures.
* Returning results back to Python.

The bridge does not perform any image processing.

It exists purely for:

* Process management.
* Data transportation.
* Error propagation.
* Cross-architecture communication.

# 7. RISC-V Wrapper Layer (wrapper.c)
wrapper.c is the entry point of the RISC-V executable.

It provides the runtime environment that allows assembly kernels to operate as part of a complete application.

Its responsibilities include:

* Parsing command-line arguments.
* Determining which image operation was requested.
* Reading raw image streams from standard input.
* Allocating input and output buffers.
* Calling the corresponding assembly function.
* Writing processed data to standard output.
* Running benchmark loops when benchmark mode is requested.
* Measuring execution time using a monotonic system clock.
* Reporting average execution time to the bridge.

The wrapper contains no image processing logic.

It only manages execution flow and provides a C interface for assembly routines.

# 8. RISC-V Assembly Layer
The assembly files contain the actual image processing algorithms.

## invert.s
Performs pixel inversion:

```
output = 255 - input
```

---

## threshold.s
Performs binary thresholding:

```
if pixel >= threshold:
    output = 255
else:
    output = 0
```

## brightness.s
Adjusts pixel brightness by adding an offset.

The result is clamped to the valid grayscale range:

```
0 <= pixel <= 255
```

This prevents integer overflow or underflow.

## blur.s
Implements a 3×3 box blur filter.

Each pixel is replaced by the average of its surrounding 3×3 neighborhood.

Boundary pixels are handled separately to prevent reading memory outside the image buffer.

# 9. Detailed Runtime Flow
## Image Generation Mode

When main.py executes:

1. Python loads a grayscale image using OpenCV.
2. The image is represented as a NumPy uint8 buffer.
3. Python passes buffer addresses to the bridge using ctypes.
4. The bridge creates pipes and launches QEMU.
5. QEMU starts the RISC-V executable.
6. The bridge streams image bytes through stdin.
7. The wrapper receives the input stream.
8. The wrapper allocates output memory.
9. The requested assembly function is called.
10. The assembly kernel modifies the image data.
11. The wrapper writes the processed bytes to stdout.
12. The bridge receives the output stream.
13. Python reconstructs the output image.
14. OpenCV saves the final image.

# 10. Benchmark Runtime Flow
When timings.py executes:

1. Python executes the reference implementation multiple times.
2. Average Python execution time is calculated.
3. Python calls the bridge benchmark API.
4. The bridge launches the RISC-V executable through QEMU.
5. The wrapper enters benchmark mode.
6. The assembly kernel is executed repeatedly.
7. The wrapper calculates the average runtime in nanoseconds.
8. The timing value is returned through stdout.
9. The bridge parses the benchmark output.
10. Python displays the final comparison table.

# 11. Build Artifacts
The project produces two important binaries.

## libbridge.so
The native x86-64 shared library loaded by Python.

Purpose:

* Provides the ctypes interface.
* Handles IPC.
* Controls QEMU execution.

## app
The RISC-V executable containing:

* wrapper.c
* RISC-V assembly kernels

This executable runs inside QEMU.

# 12. Why Stream-Based IPC Was Chosen
Earlier approaches may use temporary files to transfer image buffers between processes.

This project uses UNIX pipes instead because they provide several advantages:

* No temporary files need to be created.
* No disk I/O is required.
* Data flows directly between processes.
* Memory usage is simpler to control.
* The communication model more closely matches real operating-system process interaction.

The result is a cleaner and more efficient architecture.

# 13. Limitations and Scope
The project focuses specifically on grayscale offline image processing.

It does not currently include:

* RGB or multi-channel image processing.
* Real-time video processing.
* GPU acceleration.
* Parallel execution.
* Hardware RISC-V execution.

The purpose of the project is to study software architecture, assembly programming, and performance characteristics under emulation.

# 14. Key Concepts Demonstrated
This project demonstrates practical experience with:

* RISC-V assembly programming.
* The RISC-V calling convention.
* C and assembly interoperability.
* Python-to-C interoperability using ctypes.
* Cross-architecture execution using QEMU.
* Process management using fork and exec.
* Stream-oriented inter-process communication.
* Dynamic memory management.
* Image buffer manipulation.
* Benchmark design and performance analysis.

# 15. Final Summary
The project follows a layered architecture where each technology has a dedicated responsibility.

Python provides high-level orchestration and user interaction.

The native C bridge provides a clean interface between Python and the emulated environment while managing process creation and stream-based communication.

QEMU provides the execution environment for the RISC-V binary.

The RISC-V wrapper controls program execution, memory management, and benchmark operations.

Finally, the assembly kernels perform the actual pixel-level image transformations.

This separation of responsibilities creates a modular, maintainable, and educational systems programming project that demonstrates how high-level and low-level software components can be integrated into a complete cross-architecture application.