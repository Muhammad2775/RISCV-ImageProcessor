import numpy as np

def invert(img: np.ndarray) -> np.ndarray:
    img = np.ascontiguousarray(img, dtype=np.uint8)
    return (255 - img).astype(np.uint8)

def threshold(img: np.ndarray, T: int) -> np.ndarray:
    img = np.ascontiguousarray(img, dtype=np.uint8)
    return np.where(img >= int(T), 255, 0).astype(np.uint8)

def brightness(img: np.ndarray, B: int) -> np.ndarray:
    img = np.ascontiguousarray(img, dtype=np.uint8)
    out = img.astype(np.int16) + int(B)
    return np.clip(out, 0, 255).astype(np.uint8)

def blur(img: np.ndarray) -> np.ndarray:
    img = np.ascontiguousarray(img, dtype=np.uint8)
    h, w = img.shape
    out = img.copy()

    for y in range(1, h - 1):
        for x in range(1, w - 1):
            region = img[y - 1:y + 2, x - 1:x + 2]
            out[y, x] = int(region.sum() // 9)

    return out.astype(np.uint8)