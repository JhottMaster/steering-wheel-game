from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


def find_center_road_band(source: Image.Image, threshold: float) -> tuple[int, int]:
    rgb = source.convert("RGB")
    pixels = rgb.load()
    row_scores: list[float] = []
    for y in range(rgb.height):
        total = 0.0
        for x in range(rgb.width):
            r, g, b = pixels[x, y]
            total += (r + g + b) / 3.0
        row_scores.append(total / rgb.width)

    dark_rows = [score < threshold for score in row_scores]

    # Bridge small bright gaps caused by yellow dashed lane markings.
    bridge_gap = 24
    for y, is_dark in enumerate(dark_rows):
        if is_dark:
            continue
        top = max(0, y - bridge_gap)
        bottom = min(len(dark_rows), y + bridge_gap + 1)
        if any(dark_rows[top:y]) and any(dark_rows[y + 1:bottom]):
            dark_rows[y] = True

    groups: list[tuple[int, int]] = []
    start: int | None = None
    for y, is_dark in enumerate(dark_rows):
        if is_dark and start is None:
            start = y
        elif not is_dark and start is not None:
            groups.append((start, y - 1))
            start = None
    if start is not None:
        groups.append((start, len(dark_rows) - 1))

    center = rgb.height // 2
    road_top, road_bottom = min(
        groups,
        key=lambda group: 0 if group[0] <= center <= group[1]
        else min(abs(group[0] - center), abs(group[1] - center)),
    )
    return road_top, road_bottom


def make_road_alpha(
    source_path: Path,
    output_path: Path,
    feather_pixels: int,
    threshold: float,
    edge_inset_pixels: int,
) -> None:
    source = Image.open(source_path).convert("RGBA")
    road_top, road_bottom = find_center_road_band(source, threshold)
    road_top += edge_inset_pixels
    road_bottom -= edge_inset_pixels

    road_mask = Image.new("L", source.size, 0)
    road_pixels = road_mask.load()
    for y in range(source.height):
        if road_top <= y <= road_bottom:
            alpha = 255
        elif road_top - feather_pixels <= y < road_top:
            alpha = round(255 * (y - (road_top - feather_pixels)) / feather_pixels)
        elif road_bottom < y <= road_bottom + feather_pixels:
            alpha = round(255 * ((road_bottom + feather_pixels) - y) / feather_pixels)
        else:
            alpha = 0

        for x in range(source.width):
            road_pixels[x, y] = alpha

    transparent = Image.new("RGBA", source.size, (0, 0, 0, 0))
    output = Image.composite(source, transparent, road_mask)
    output.save(output_path)
    print(f"Detected road band y={road_top}..{road_bottom}")


def make_preview(background_path: Path, road_path: Path, preview_path: Path, repeat_x: int) -> None:
    background = Image.open(background_path).convert("RGBA")
    road = Image.open(road_path).convert("RGBA")
    preview = Image.new("RGBA", (background.width * repeat_x, background.height))

    for x in range(repeat_x):
      preview.paste(background, (x * background.width, 0))

    for x in range(repeat_x):
      resized_road = road
      if road.size != background.size:
          resized_road = road.resize(background.size, Image.LANCZOS)
      preview.alpha_composite(resized_road, (x * background.width, 0))

    if max(preview.size) > 1600:
        preview.thumbnail((1600, 900), Image.LANCZOS)

    if preview_path.suffix.lower() in {".jpg", ".jpeg"}:
        preview.convert("RGB").save(preview_path, quality=90)
    else:
        preview.save(preview_path)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Remove green terrain from a road tile and feather the road alpha edge."
    )
    parser.add_argument("source", type=Path, help="Input road tile image.")
    parser.add_argument("output", type=Path, help="Output transparent road tile PNG.")
    parser.add_argument("--feather-pixels", type=int, default=5, help="Alpha feather size.")
    parser.add_argument(
        "--edge-inset-pixels",
        type=int,
        default=0,
        help="Move the alpha transition inward from the detected road edge.",
    )
    parser.add_argument(
        "--road-threshold",
        type=float,
        default=130.0,
        help="Average row brightness threshold used to detect the centered road.",
    )
    parser.add_argument("--preview-background", type=Path, help="Optional terrain image for preview.")
    parser.add_argument("--preview", type=Path, help="Optional composited preview output.")
    parser.add_argument("--preview-repeat-x", type=int, default=3, help="Horizontal preview repeats.")
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    make_road_alpha(
        args.source,
        args.output,
        args.feather_pixels,
        args.road_threshold,
        args.edge_inset_pixels,
    )

    if args.preview and args.preview_background:
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        make_preview(args.preview_background, args.output, args.preview, args.preview_repeat_x)

    print(f"Wrote transparent road tile: {args.output}")
    if args.preview and args.preview_background:
        print(f"Wrote preview: {args.preview}")


if __name__ == "__main__":
    main()
