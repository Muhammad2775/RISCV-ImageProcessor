import ctypes
import time
from pathlib import Path

import cv2
import numpy as np

from python_implementation import blur as py_blur
from python_implementation import brightness as py_brightness
from python_implementation import invert as py_invert
from python_implementation import threshold as py_threshold

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

    lib.bridge_last_error.argtypes = []
    lib.bridge_last_error.restype = ctypes.c_char_p

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

def py_bench(fn, img: np.ndarray, *args, repeats: int = 10):
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
    input_ptr = img.ctypes.data_as(U8_PTR)

    lib = load_bridge()

    operations = [
        {
            "name": "invert",
            "py_fn": py_invert,
            "py_args": (),
            "py_repeats": 10,
            "asm_fn": lib.bridge_bench_invert,
            "asm_args": (size, 200),
        },
        {
            "name": "threshold",
            "py_fn": py_threshold,
            "py_args": (128,),
            "py_repeats": 10,
            "asm_fn": lib.bridge_bench_threshold,
            "asm_args": (size, 128, 200),
        },
        {
            "name": "brightness",
            "py_fn": py_brightness,
            "py_args": (40,),
            "py_repeats": 10,
            "asm_fn": lib.bridge_bench_brightness,
            "asm_args": (size, 40, 200),
        },
        {
            "name": "blur",
            "py_fn": py_blur,
            "py_args": (),
            "py_repeats": 3,
            "asm_fn": lib.bridge_bench_blur,
            "asm_args": (w, h, 25),
        },
    ]

    print(f"{'operation':<12} {'python ms/op':>14} {'asm ms/op':>14} {'speedup':>10}")
    print("-" * 56)

    for op in operations:
        py_ms, _ = py_bench(op["py_fn"], img, *op["py_args"], repeats=op["py_repeats"])

        asm_ns = op["asm_fn"](input_ptr, *op["asm_args"])
        if asm_ns < 0:
            raise RuntimeError(bridge_error(lib))

        asm_ms = asm_ns / 1e6
        speedup = py_ms / asm_ms if asm_ms > 0 else float("inf")

        print(f"{op['name']:<12} {py_ms:14.3f} {asm_ms:14.3f} {speedup:10.2f}x")

if __name__ == "__main__":
    main()
