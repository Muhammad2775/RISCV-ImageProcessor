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
OUTPUT_DIR = ROOT / "images" / "output"

U8_PTR = ctypes.POINTER(ctypes.c_uint8)

def ensure_environment() -> None:
    if not BRIDGE_PATH.exists():
        raise FileNotFoundError(f"Bridge library not found: {BRIDGE_PATH}")
    if not APP_PATH.exists():
        raise FileNotFoundError(f"RISC-V app not found: {APP_PATH}")
    if not INPUT_IMG.exists():
        raise FileNotFoundError(f"Input image not found: {INPUT_IMG}")
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

def load_bridge() -> ctypes.CDLL:
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

def timed(fn):
    start = time.perf_counter_ns()
    result = fn()
    elapsed_ms = (time.perf_counter_ns() - start) / 1e6
    return result, elapsed_ms

def call_bridge(lib, bridge_fn, img: np.ndarray, *args) -> np.ndarray:
    input_arr = np.ascontiguousarray(img, dtype=np.uint8)
    output_arr = np.empty_like(input_arr)

    rc = bridge_fn(
        input_arr.ctypes.data_as(U8_PTR),
        output_arr.ctypes.data_as(U8_PTR),
        *args,
    )
    if rc != 0:
        raise RuntimeError(bridge_error(lib))

    return output_arr

def save_output(name: str, img: np.ndarray) -> Path:
    out_path = OUTPUT_DIR / f"sample_01_{name}.png"
    ok = cv2.imwrite(str(out_path), img)
    if not ok:
        raise RuntimeError(f"Failed to write image: {out_path}")
    return out_path

def main() -> None:
    ensure_environment()
    lib = load_bridge()

    img = cv2.imread(str(INPUT_IMG), cv2.IMREAD_GRAYSCALE)
    if img is None:
        raise RuntimeError(f"Failed to load image: {INPUT_IMG}")

    img = np.ascontiguousarray(img, dtype=np.uint8)
    h, w = img.shape
    size = h * w

    operations = [
        ("invert", py_invert, (), lib.bridge_invert, (size,)),
        ("threshold", py_threshold, (128,), lib.bridge_threshold, (size, 128)),
        ("brightness", py_brightness, (40,), lib.bridge_brightness, (size, 40)),
        ("blur", py_blur, (), lib.bridge_blur, (w, h)),
    ]

    for name, py_fn, py_args, asm_fn, asm_args in operations:
        py_out, py_ms = timed(lambda fn=py_fn, args=py_args: fn(img, *args))
        asm_out, asm_ms = timed(lambda fn=asm_fn, args=asm_args: call_bridge(lib, fn, img, *args))

        save_output(name, asm_out)
        match = np.array_equal(py_out, asm_out)
        print(f"[{name}] python={py_ms:.3f} ms | asm={asm_ms:.3f} ms | match={match}")

if __name__ == "__main__":
    main()
