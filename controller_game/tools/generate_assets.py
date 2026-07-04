from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SPRITES = ROOT / "assets" / "sprites"


def rounded_rect(draw: ImageDraw.ImageDraw, xy, radius: int, fill, outline=None, width: int = 1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def make_terrain_tiles():
    size = 256
    image = Image.new("RGBA", (size, size), (68, 142, 134, 255))
    draw = ImageDraw.Draw(image)

    for y in range(0, size, 8):
        color = (61, 132, 126, 70) if (y // 8) % 2 == 0 else (82, 155, 145, 58)
        draw.line((0, y, size, y), fill=color, width=3)
    for x in range(0, size, 10):
        color = (48, 112, 108, 45) if (x // 10) % 2 == 0 else (95, 169, 157, 42)
        draw.line((x, 0, x, size), fill=color, width=2)
    for i in range(28):
        x = (i * 37) % size
        y = (i * 53) % size
        draw.ellipse((x - 1, y - 1, x + 2, y + 2), fill=(215, 216, 179, 36))

    image = image.filter(ImageFilter.UnsharpMask(radius=1, percent=105, threshold=3))
    image.save(SPRITES / "terrain_carpet.png")

    mirror_x = image.transpose(Image.FLIP_LEFT_RIGHT)
    mirror_y = image.transpose(Image.FLIP_TOP_BOTTOM)
    mirror_xy = mirror_x.transpose(Image.FLIP_TOP_BOTTOM)
    tile = Image.new("RGBA", (size * 2, size * 2))
    tile.paste(image, (0, 0))
    tile.paste(mirror_x, (size, 0))
    tile.paste(mirror_y, (0, size))
    tile.paste(mirror_xy, (size, size))
    tile.save(SPRITES / "terrain_carpet_tilemirror.png")


def make_car():
    image = Image.new("RGBA", (128, 160), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse((28, 118, 54, 148), fill=(38, 38, 42, 255))
    draw.ellipse((74, 118, 100, 148), fill=(38, 38, 42, 255))
    draw.ellipse((26, 18, 54, 48), fill=(38, 38, 42, 255))
    draw.ellipse((74, 18, 102, 48), fill=(38, 38, 42, 255))
    rounded_rect(draw, (28, 18, 100, 146), 34, fill=(222, 79, 57, 255), outline=(111, 50, 45, 255), width=5)
    rounded_rect(draw, (40, 48, 88, 94), 14, fill=(134, 209, 224, 255), outline=(63, 110, 132, 255), width=4)
    draw.polygon([(64, 4), (42, 34), (86, 34)], fill=(242, 192, 84, 255))
    rounded_rect(draw, (49, 106, 79, 132), 8, fill=(245, 226, 178, 255))
    image.save(SPRITES / "toy_sports_car.png")


def make_coin():
    image = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse((10, 12, 86, 88), fill=(235, 178, 47, 255), outline=(142, 97, 28, 255), width=5)
    draw.ellipse((22, 24, 74, 76), fill=(255, 219, 92, 255))
    star = []
    cx, cy = 48, 49
    for i in range(10):
        radius = 23 if i % 2 == 0 else 10
        angle = -math.pi / 2 + i * math.pi / 5
        star.append((cx + math.cos(angle) * radius, cy + math.sin(angle) * radius))
    draw.polygon(star, fill=(255, 246, 185, 255), outline=(168, 115, 33, 255))
    image.save(SPRITES / "coin_star.png")


def main():
    SPRITES.mkdir(parents=True, exist_ok=True)
    make_terrain_tiles()
    make_car()
    make_coin()
    print(f"Generated assets in {SPRITES}")


if __name__ == "__main__":
    main()
