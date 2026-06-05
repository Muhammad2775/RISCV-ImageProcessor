import ctypes
import os
import time
from pathlib import Path

import cv2
import numpy as np

from pure_python_ref import invert as py_invert
from pure_python_ref import threshold as py_threshold
from pure_python_ref import brightness as py_brightness
from pure_python_ref import blur as py_blur

BASE = Path(__file__).resolve().parent
ROOT = BASE.parent
BUILD = ROOT / "build"

APP_PATH = BUILD / "app"
BRIDGE_PATH = BUILD / "libbridge.so"

INPUT_IMG = ROOT / "images" / "input" / "sample_01.png"
OUTPUT_DIR = ROOT / "images" / "output"

U8_PTR = ctypes.POINTER(ctypes.c_uint8)

def load_bridge() -> ctypes.CDLL:
    if not BRIDGE_PATH.exists():
        raise FileNotFoundError(f"Bridge library not found: {BRIDGE_PATH}")

    lib = ctypes.CDLL(str(BRIDGE_PATH))

    lib.bridge_set_qemu_path.argtypes = [ctypes.c_char_p]
    lib.bridge_set_qemu_path.restype = None

    lib.bridge_set_app_path.argtypes = [ctypes.c_char_p]
    lib.bridge_set_app_path.restype = None

    lib.bridge_last_error.argtypes = []
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

def bridge_error(lib: ctypes.CDLL) -> str:
    msg = lib.bridge_last_error()
    return msg.decode() if msg else "unknown bridge error"

def ensure_environment() -> None:
    if not INPUT_IMG.exists():
        raise FileNotFoundError(f"Input image not found: {INPUT_IMG}")
    if not APP_PATH.exists():
        raise FileNotFoundError(f"RISC-V app not found: {APP_PATH}")
    if not BRIDGE_PATH.exists():
        raise FileNotFoundError(f"Bridge library not found: {BRIDGE_PATH}")
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

def as_u8_ptr(arr: np.ndarray) -> U8_PTR:
    arr = np.ascontiguousarray(arr, dtype=np.uint8)
    return arr.ctypes.data_as(U8_PTR)

def py_time_ms(fn, img: np.ndarray, *args) -> tuple[float, np.ndarray]:
    start = time.perf_counter_ns()
    out = fn(img, *args)
    elapsed_ms = (time.perf_counter_ns() - start) / 1e6
    return elapsed_ms, out

def bridge_time_ms(lib: ctypes.CDLL, fn, img: np.ndarray, *args) -> tuple[float, np.ndarray]:
    input_arr = np.ascontiguousarray(img, dtype=np.uint8)
    output_arr = np.empty_like(input_arr)

    in_ptr = as_u8_ptr(input_arr)
    out_ptr = as_u8_ptr(output_arr)

    start = time.perf_counter_ns()
    rc = fn(in_ptr, out_ptr, *args)
    elapsed_ms = (time.perf_counter_ns() - start) / 1e6

    if rc != 0:
        raise RuntimeError(bridge_error(lib))

    return elapsed_ms, output_arr.reshape(img.shape)

def save_output(name: str, img: np.ndarray) -> Path:
    out_path = OUTPUT_DIR / f"sample_01_{name}.png"
    ok = cv2.imwrite(str(out_path), img)
    if not ok:
        raise RuntimeError(f"Failed to write image: {out_path}")
    return out_path

def process_all() -> None:
    lib = load_bridge()

    img = cv2.imread(str(INPUT_IMG), cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise RuntimeError(f"Failed to load image: {INPUT_IMG}")

    img = np.ascontiguousarray(img, dtype=np.uint8)
    h, w = img.shape
    size = h * w

    # invert
    py_ms, py_out = py_time_ms(py_invert, img)
    asm_ms, asm_out = bridge_time_ms(lib, lib.bridge_invert, img, size)
    save_output("invert", asm_out)
    print(f"[invert] python={py_ms:.3f} ms | asm={asm_ms:.3f} ms | match={np.array_equal(py_out, asm_out)}")

    # threshold
    T = 128
    py_ms, py_out = py_time_ms(py_threshold, img, T)
    asm_ms, asm_out = bridge_time_ms(lib, lib.bridge_threshold, img, size, ctypes.c_uint8(T))
    save_output("threshold", asm_out)
    print(f"[threshold] python={py_ms:.3f} ms | asm={asm_ms:.3f} ms | match={np.array_equal(py_out, asm_out)}")

    # brightness
    B = 40
    py_ms, py_out = py_time_ms(py_brightness, img, B)
    asm_ms, asm_out = bridge_time_ms(lib, lib.bridge_brightness, img, size, B)
    save_output("brightness", asm_out)
    print(f"[brightness] python={py_ms:.3f} ms | asm={asm_ms:.3f} ms | match={np.array_equal(py_out, asm_out)}")

    # blur
    py_ms, py_out = py_time_ms(py_blur, img)
    asm_ms, asm_out = bridge_time_ms(lib, lib.bridge_blur, img, w, h)
    save_output("blur", asm_out)
    print(f"[blur] python={py_ms:.3f} ms | asm={asm_ms:.3f} ms | match={np.array_equal(py_out, asm_out)}")

if __name__ == "__main__":
    ensure_environment()
    process_all()