from collections import deque
from pathlib import Path
import argparse

from PIL import Image


def is_dark(pixel, threshold):
    r, g, b = pixel[:3]
    return max(r, g, b) <= threshold


def apply_border_matte_alpha(path, seed_threshold, grow_threshold, transparent_threshold,
                             feather_threshold):
    image = Image.open(path).convert("RGBA")
    source_rgb = image.convert("RGB")
    width, height = image.size
    source_pixels = source_rgb.load()
    visited = [[False] * width for _ in range(height)]
    matte = [[False] * width for _ in range(height)]
    queue = deque()

    def enqueue_if_seed(x, y):
        if not visited[y][x] and is_dark(source_pixels[x, y], seed_threshold):
            visited[y][x] = True
            queue.append((x, y))

    for x in range(width):
        enqueue_if_seed(x, 0)
        enqueue_if_seed(x, height - 1)
    for y in range(height):
        enqueue_if_seed(0, y)
        enqueue_if_seed(width - 1, y)

    while queue:
        x, y = queue.popleft()
        matte[y][x] = True
        for next_x, next_y in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= next_x < width and 0 <= next_y < height:
                if not visited[next_y][next_x] and is_dark(source_pixels[next_x, next_y],
                                                           grow_threshold):
                    visited[next_y][next_x] = True
                    queue.append((next_x, next_y))

    output = image.copy()
    output_pixels = output.load()
    transparent = 0
    partial = 0
    opaque = 0
    for y in range(height):
        for x in range(width):
            r, g, b, _ = output_pixels[x, y]
            alpha = 255
            if matte[y][x]:
                brightness = max(r, g, b)
                if brightness <= transparent_threshold:
                    alpha = 0
                elif brightness < feather_threshold:
                    t = (brightness - transparent_threshold) / (
                        feather_threshold - transparent_threshold
                    )
                    alpha = int(max(0.0, min(1.0, t)) * 255)
            output_pixels[x, y] = (r, g, b, alpha)
            if alpha == 0:
                transparent += 1
            elif alpha == 255:
                opaque += 1
            else:
                partial += 1

    output.save(path)
    return transparent, partial, opaque


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Make only border-connected dark matte pixels transparent. "
            "Dark pixels inside the road art remain fully opaque."
        )
    )
    parser.add_argument("images", nargs="+", type=Path)
    parser.add_argument("--seed-threshold", type=int, default=34)
    parser.add_argument("--grow-threshold", type=int, default=42)
    parser.add_argument("--transparent-threshold", type=int, default=18)
    parser.add_argument("--feather-threshold", type=int, default=42)
    args = parser.parse_args()

    for path in args.images:
        transparent, partial, opaque = apply_border_matte_alpha(
            path,
            args.seed_threshold,
            args.grow_threshold,
            args.transparent_threshold,
            args.feather_threshold,
        )
        print(f"{path}: transparent={transparent} partial={partial} opaque={opaque}")


if __name__ == "__main__":
    main()
