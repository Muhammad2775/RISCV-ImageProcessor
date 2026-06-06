import ctypes
import os
import time
from pathlib import Path

import cv2
import numpy as np

from python_implementation import invert as py_invert
from python_implementation import threshold as py_threshold
from python_implementation import brightness as py_brightness
from python_implementation import blur as py_blur

BASE = Path(__file__).resolve().parent
ROOT = BASE.parent
BUILD = ROOT / "build"

APP_PATH = BUILD / "app"
BRIDGE_PATH = BUILD / "libbridge.so"
INPUT_IMG = ROOT / "images" / "input" / "sample_01.png"

U8_PTR = ctypes.POINTER(ctypes.c_uint8)

def load_bridge() -> ctypes.CDLL:
    lib = ctypes.CDLL(str(BRIDGE_PATH))

    lib.bridge_set_qemu_path.argtypes = [ctypes.c_char_p]
    lib.bridge_set_qemu_path.restype = None

    lib.bridge_set_app_path.argtypes = [ctypes.c_char_p]
    lib.bridge_set_app_path.restype = None

    lib.bridge_last_error.restype = ctypes.c_char_p

    lib.bridge_invert.argtypes = [U8_PTR, U8_PTR, ctypes.c_int]
    lib.bridge_invert.restype = ctypes.c_int

    lib.bridge_threshold.argtypes = [U8_PTR, U8_PTR, ctypes.c_int, ctypes.c_uint8]
    lib.bridge_threshold.restype = ctypes.c_int

    lib.bridge_brightness.argtypes = [U8_PTR, U8_PTR, ctypes.c_int, ctypes.c_int]
    lib.bridge_brightness.restype = ctypes.c_int

    lib.bridge_blur.argtypes = [U8_PTR, U8_PTR, ctypes.c_int, ctypes.c_int]
    lib.bridge_blur.restype = ctypes.c_int

    lib.bridge_bench_invert.argtypes = [U8_PTR, ctypes.c_int, ctypes.c_int]
    lib.bridge_bench_invert.restype = ctypes.c_double

    lib.bridge_bench_threshold.argtypes = [U8_PTR, ctypes.c_int, ctypes.c_uint8, ctypes.c_int]
    lib.bridge_bench_threshold.restype = ctypes.c_double

    lib.bridge_bench_brightness.argtypes = [U8_PTR, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.bridge_bench_brightness.restype = ctypes.c_double

    lib.bridge_bench_blur.argtypes = [U8_PTR, ctypes.c_int, ctypes.c_int, ctypes.c_int]
    lib.bridge_bench_blur.restype = ctypes.c_double

    lib.bridge_set_qemu_path(b"qemu-riscv64")
    lib.bridge_set_app_path(str(APP_PATH).encode())

    return lib

def py_bench(fn, img: np.ndarray, *args, repeats: int = 10) -> tuple[float, np.ndarray]:
    out = None
    start = time.perf_counter_ns()
    for _ in range(repeats):
        out = fn(img, *args)
    elapsed_ms = (time.perf_counter_ns() - start) / 1e6 / repeats
    return elapsed_ms, out

def main() -> None:
    if not BRIDGE_PATH.exists():
        raise FileNotFoundError(f"Bridge library not found: {BRIDGE_PATH}")
    if not APP_PATH.exists():
        raise FileNotFoundError(f"RISC-V app not found: {APP_PATH}")

    img = cv2.imread(str(INPUT_IMG), cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise FileNotFoundError(f"Could not load image: {INPUT_IMG}")

    img = np.ascontiguousarray(img, dtype=np.uint8)
    h, w = img.shape
    size = h * w

    lib = load_bridge()

    operations = [
        ("invert", py_invert, (), 25, 200),
        ("threshold", py_threshold, (np.uint8(128),), 25, 200),
        ("brightness", py_brightness, (40,), 25, 200),
        ("blur", py_blur, (), 3, 25),
    ]

    print(f"{'operation':<12} {'python ms/op':>14} {'asm ms/op':>14} {'speedup':>10}")
    print("-" * 56)

    for name, py_fn, py_args, py_repeats, asm_iterations in operations:
        py_ms, py_out = py_bench(py_fn, img, *py_args, repeats=py_repeats)
        if name == "invert":
            asm_ns = lib.bridge_bench_invert(img.ctypes.data_as(U8_PTR), size, asm_iterations)
        elif name == "threshold":
            asm_ns = lib.bridge_bench_threshold(img.ctypes.data_as(U8_PTR), size, np.uint8(128), asm_iterations)
        elif name == "brightness":
            asm_ns = lib.bridge_bench_brightness(img.ctypes.data_as(U8_PTR), size, 40, asm_iterations)
        elif name == "blur":
            asm_ns = lib.bridge_bench_blur(img.ctypes.data_as(U8_PTR), w, h, asm_iterations)
        else:
            raise RuntimeError(f"Unknown operation benchmark for: {name}")

        if asm_ns < 0:
            err = lib.bridge_last_error()
            raise RuntimeError(err.decode() if err else "unknown bridge error")

        asm_ms = asm_ns / 1e6
        speedup = py_ms / asm_ms if asm_ms > 0 else float("inf")

        print(f"{name:<12} {py_ms:14.3f} {asm_ms:14.3f} {speedup:10.2f}x")

if __name__ == "__main__":
    main()
