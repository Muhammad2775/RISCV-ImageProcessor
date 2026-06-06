import numpy as np

def invert(img: np.ndarray) -> np.ndarray:
    h, w = img.shape
    out = np.empty_like(img)

    for y in range(h):
        for x in range(w):
            out[y, x] = 255 - int(img[y, x])

    return out

def threshold(img: np.ndarray, T: int) -> np.ndarray:
    h, w = img.shape
    out = np.empty_like(img)

    for y in range(h):
        for x in range(w):
            out[y, x] = 255 if img[y, x] >= T else 0

    return out

def brightness(img: np.ndarray, B: int) -> np.ndarray:
    h, w = img.shape
    out = np.empty_like(img)

    for y in range(h):
        for x in range(w):
            value = int(img[y, x]) + B

            if value > 255: value = 255
            elif value < 0: value = 0

            out[y, x] = value

    return out

def blur(img: np.ndarray) -> np.ndarray:
    h, w = img.shape
    out = img.copy()

    for y in range(1, h - 1):
        for x in range(1, w - 1):
            total = 0

            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    total += int(img[y + dy, x + dx])

            out[y, x] = total // 9

    return out