from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


def make_mirror_tile(source_path: Path, output_path: Path) -> None:
    source = Image.open(source_path).convert("RGBA")
    mirror_x = source.transpose(Image.FLIP_LEFT_RIGHT)
    mirror_y = source.transpose(Image.FLIP_TOP_BOTTOM)
    mirror_xy = mirror_x.transpose(Image.FLIP_TOP_BOTTOM)

    output = Image.new("RGBA", (source.width * 2, source.height * 2))
    output.paste(source, (0, 0))
    output.paste(mirror_x, (source.width, 0))
    output.paste(mirror_y, (0, source.height))
    output.paste(mirror_xy, (source.width, source.height))
    output.save(output_path)


def make_preview(tile_path: Path, preview_path: Path, repeat_count: int, max_size: int) -> None:
    tile = Image.open(tile_path).convert("RGBA")
    preview = Image.new("RGBA", (tile.width * repeat_count, tile.height * repeat_count))
    for y in range(repeat_count):
        for x in range(repeat_count):
            preview.paste(tile, (x * tile.width, y * tile.height))

    if max(preview.size) > max_size:
        preview.thumbnail((max_size, max_size), Image.LANCZOS)

    if preview_path.suffix.lower() in {".jpg", ".jpeg"}:
        preview = preview.convert("RGB")
        preview.save(preview_path, quality=88)
    else:
        preview.save(preview_path)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Create a seamless 2x2 mirror tile from an input texture."
    )
    parser.add_argument("source", type=Path, help="Input image path.")
    parser.add_argument("output", type=Path, help="Output seamless tile image path.")
    parser.add_argument(
        "--preview",
        type=Path,
        help="Optional preview image path showing the output tile repeated.",
    )
    parser.add_argument("--repeat-count", type=int, default=3, help="Preview repeat count.")
    parser.add_argument("--preview-max-size", type=int, default=1200, help="Max preview edge size.")
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    make_mirror_tile(args.source, args.output)

    if args.preview:
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        make_preview(args.output, args.preview, args.repeat_count, args.preview_max_size)

    print(f"Wrote mirror tile: {args.output}")
    if args.preview:
        print(f"Wrote preview: {args.preview}")


if __name__ == "__main__":
    main()
