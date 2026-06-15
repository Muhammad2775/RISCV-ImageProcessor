# Project Setup & Build Guide

## Introduction

This guide provides complete instructions for setting up, building, and running the RISC-V Assembly Image Processing project.

The project was developed and tested using:

* Windows 10/11
* WSL2 (Ubuntu)
* Python 3
* GCC compiler
* RISC-V GNU Toolchain
* QEMU RISC-V Emulator

The repository supports multiple build workflows. The main commands in this guide are sufficient to build and run the project successfully. On native Linux systems, you may use CMake. Premake or Lua-based Make workflows can also be used if you prefer them. If you are using CMake on Windows, follow the setup steps in the next section to configure CMake, Ninja, and VS Code correctly.

## Before You Begin

### Build-System Note

* CMake is primarily intended for native Linux users in this project setup.
* The direct compiler commands shown in this guide are enough to build and run the project.
* If you prefer CMake, you can still use it on Windows, provided it is installed and configured correctly.
* Premake or a Lua-based Make setup can also be used as an alternative build system.

### Required VS Code Extensions

For Windows, WSL, and general project work, install the following extensions in VS Code:

* Microsoft **WSL**
* Microsoft **Remote - Tunnels**
* Microsoft **Remote Explorer**
* Microsoft **C/C++**
* Microsoft **Python**
* **CMake Tools**

These extensions help VS Code work correctly in both normal Windows mode and WSL mode.

### Optional CMake and Ninja Setup for Windows Users

If you want to use CMake on Windows and it is not already installed, configure it as follows:

1. Download the latest Windows x64 CMake installer from the official download page:

   [https://cmake.org/download/](https://cmake.org/download/)

2. Install the Windows x64 installer. The filename will look similar to:

   `cmake-4.3.3-windows-x86_64.msi`

3. After installation, go to:

   `C:\Program Files\CMake\bin`

4. Confirm that `cmake.exe` exists in that folder.

5. Copy that full path into **System Environment Variables → Path**.

6. Save the changes, then open VS Code and add that same path in the CMake Tools setting, as shown in the screenshot.

### Ninja Requirement

This setup assumes that Ninja is available on your system.

Check Ninja from Command Prompt or PowerShell:

```powershell
ninja --version
```

If Ninja is installed, a version number is shown.

If Ninja is not installed, you will see an error similar to:

```text
'ninja' is not recognized as an internal or external command, operable program or batch file.
```

If Ninja is missing, download it from:

[https://github.com/ninja-build/ninja/releases](https://github.com/ninja-build/ninja/releases)

Download the file similar to `ninja-win.zip`, extract it, and make `ninja.exe` available on your PATH. A simple approach is to place `ninja.exe` in a folder that is already on the PATH, or add the folder containing `ninja.exe` to the PATH. Restart your PC after making the change.

## System Requirements

### Operating System

Recommended:

* Windows 11/10 with WSL2 support

Alternative:

* Native Ubuntu Linux

## Installing WSL2

### Enable WSL

Open PowerShell as Administrator:

```powershell
wsl --install
```

Restart the computer when prompted.

### Verify Installation

Open PowerShell:

```powershell
wsl --status
```

Expected output should indicate:

```text
Default Version: 2
```

### Install Ubuntu

If Ubuntu was not installed automatically:

```powershell
wsl --install -d Ubuntu
```

Launch Ubuntu and create a username and password.

## Updating Ubuntu

Inside WSL:

```bash
sudo apt update
sudo apt upgrade -y
```

## Installing Build Tools

Install common development packages:

```bash
sudo apt install -y \
build-essential \
git \
cmake \
python3 \
python3-pip
```

Verify:

```bash
gcc --version
python3 --version
```

## Installing RISC-V Toolchain

Install the RISC-V cross compiler:

```bash
sudo apt install -y \
gcc-riscv64-linux-gnu \
binutils-riscv64-linux-gnu
```

Verify:

```bash
riscv64-linux-gnu-gcc --version
```

## Installing QEMU

Install QEMU user-mode emulation:

```bash
sudo apt install -y \
qemu-user \
qemu-user-static
```

Verify:

```bash
qemu-riscv64 --version
```

## Installing Python Dependencies

Install required Python libraries:

```bash
pip3 install numpy opencv-python
```

Verify:

```bash
python3 -c "import cv2"
python3 -c "import numpy"
```

No errors should be displayed.

## Cloning the Repository

Clone the project:

```bash
git clone <repository-url>
```

Enter the repository:

```bash
cd RISCV_Project
```

## Repository Layout

```text
RISCV_Project/
│
├── asm/
├── c/
├── python/
├── images/
├── build/
├── README.md
├── Technical Project Guide.md
└── Project Setup Guide.md
```

## Building the Project

### Create Build Directory

```bash
mkdir -p build
```

### Build Native Bridge Library

Compile the x86-64 bridge used by Python ctypes:

```bash
gcc -O2 \
-fPIC \
-shared \
-Wall \
-Wextra \
c/bridge.c \
-o build/libbridge.so
```

Expected output:

```text
build/libbridge.so
```

### Build RISC-V Executable

Compile `wrapper.c` together with all assembly kernels:

```bash
riscv64-linux-gnu-gcc \
-O2 \
-march=rv64imafd \
-mabi=lp64d \
-mno-relax \
c/wrapper.c \
asm/*.s \
-o build/app
```

Expected output:

```text
build/app
```

## Verifying the RISC-V Binary

Check the executable:

```bash
file build/app
```

Expected output:

```text
ELF 64-bit LSB executable
UCB RISC-V
```

This confirms successful RISC-V compilation.

## Testing QEMU

Run:

```bash
qemu-riscv64 \
-L /usr/riscv64-linux-gnu \
build/app
```

Expected output:

```text
Usage:
...
```

This confirms:

* QEMU is functioning
* The RISC-V executable launches correctly

## Preparing Input Images

Place grayscale input images in:

```text
images/input/
```

Example:

```text
images/input/sample_01.png
```

Output images will be generated in:

```text
images/output/
```

## Running Image Generation

Execute:

```bash
python3 python/main.py
```

Expected output:

```text
[invert] python=...
[threshold] python=...
[brightness] python=...
[blur] python=...
```

Generated files:

```text
sample_01_invert.png
sample_01_threshold.png
sample_01_brightness.png
sample_01_blur.png
```

These files will appear inside:

```text
images/output/
```

## Running Benchmarks

Execute:

```bash
python3 python/timings.py
```

Expected output:

```text
operation      python ms/op      asm ms/op
--------------------------------------------------------
invert
threshold
brightness
blur
```

The benchmark compares:

* Native Python implementations
* RISC-V Assembly implementations

and reports execution times and relative speedups.

## Common Build Errors

### Missing RISC-V Compiler

Error:

```text
riscv64-linux-gnu-gcc: command not found
```

Solution:

```bash
sudo apt install gcc-riscv64-linux-gnu
```

### Missing QEMU

Error:

```text
qemu-riscv64: command not found
```

Solution:

```bash
sudo apt install qemu-user
```

### Missing Python Modules

Error:

```text
ModuleNotFoundError
```

Solution:

```bash
pip3 install numpy opencv-python
```

### Bridge Library Missing

Error:

```text
Bridge library not found
```

Solution:

Rebuild:

```bash
gcc -O2 -fPIC -shared c/bridge.c -o build/libbridge.so
```

### RISC-V Application Missing

Error:

```text
RISC-V app not found
```

Solution:

Rebuild:

```bash
riscv64-linux-gnu-gcc \
-march=rv64imafd \
-mabi=lp64d \
-mno-relax \
c/wrapper.c \
asm/*.s \
-o build/app
```

## Clean Rebuild

Delete previous build artifacts:

```bash
rm -rf build/*
```

Rebuild:

```bash
mkdir -p build
```

```bash
gcc -O2 -fPIC -shared c/bridge.c -o build/libbridge.so
```

```bash
riscv64-linux-gnu-gcc \
-march=rv64imafd \
-mabi=lp64d \
-mno-relax \
c/wrapper.c \
asm/*.s \
-o build/app
```

## Validation Checklist

Before running the project verify:

* WSL2 installed
* Ubuntu updated
* GCC installed
* RISC-V toolchain installed
* QEMU installed
* Python dependencies installed
* `build/libbridge.so` exists
* `build/app` exists
* Input image exists
* QEMU launches successfully
* CMake and Ninja are configured only if you plan to use the CMake-based workflow

## Expected Workflow

```text
Clone Repository
        │
        ▼
Install Dependencies
        │
        ▼
Build Bridge Library
        │
        ▼
Build RISC-V Application
        │
        ▼
Verify QEMU Execution
        │
        ▼
Run main.py
        │
        ▼
Generate Output Images
        │
        ▼
Run timings.py
        │
        ▼
Collect Performance Results
```

## Conclusion

Following this guide will produce a fully functional development environment capable of compiling RISC-V assembly code, executing it through QEMU emulation, generating processed images, and benchmarking Python and Assembly implementations. The setup intentionally separates host-side execution from emulated RISC-V execution, allowing the project to demonstrate cross-architecture software integration while remaining portable and reproducible.
