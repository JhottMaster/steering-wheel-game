from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SPRITES = ROOT / "assets" / "sprites"


def draw_dashed_line(draw: ImageDraw.ImageDraw, points, fill, width: int, dash: int = 18, gap: int = 18):
    for a, b in zip(points, points[1:]):
        ax, ay = a
        bx, by = b
        dx = bx - ax
        dy = by - ay
        length = math.hypot(dx, dy)
        if length == 0:
            continue
        ux = dx / length
        uy = dy / length
        pos = 0.0
        while pos < length:
            end = min(pos + dash, length)
            draw.line((ax + ux * pos, ay + uy * pos, ax + ux * end, ay + uy * end), fill=fill, width=width)
            pos += dash + gap


def rounded_rect(draw: ImageDraw.ImageDraw, xy, radius: int, fill, outline=None, width: int = 1):
    draw.rounded_rectangle(xy, radius=radius, fill=fill, outline=outline, width=width)


def make_map():
    size = 1024
    image = Image.new("RGBA", (size, size), (68, 142, 134, 255))
    draw = ImageDraw.Draw(image)

    # Woven carpet texture.
    for y in range(0, size, 8):
        color = (61, 132, 126, 55) if (y // 8) % 2 == 0 else (82, 155, 145, 45)
        draw.line((0, y, size, y), fill=color, width=3)
    for x in range(0, size, 10):
        color = (48, 112, 108, 35) if (x // 10) % 2 == 0 else (95, 169, 157, 35)
        draw.line((x, 0, x, size), fill=color, width=2)

    # Soft play areas.
    draw.ellipse((668, 64, 944, 250), fill=(98, 176, 116, 255), outline=(48, 125, 74, 255), width=8)
    draw.ellipse((700, 356, 942, 570), fill=(96, 169, 210, 255), outline=(56, 124, 164, 255), width=8)
    draw.rounded_rectangle((68, 658, 276, 884), radius=34, fill=(111, 178, 92, 255), outline=(64, 122, 62, 255), width=8)

    # Road rug loop.
    road = [(162, 820), (162, 578), (264, 454), (462, 454), (598, 350), (598, 172),
            (810, 172), (858, 360), (770, 492), (592, 544), (486, 688), (640, 830),
            (838, 824)]
    draw.line(road, fill=(43, 49, 55, 255), width=96, joint="curve")
    draw.line(road, fill=(74, 82, 88, 255), width=76, joint="curve")
    draw_dashed_line(draw, road, fill=(242, 229, 164, 255), width=8, dash=28, gap=24)

    # Cross roads.
    cross = [(126, 318), (330, 318), (482, 214)]
    draw.line(cross, fill=(43, 49, 55, 255), width=88, joint="curve")
    draw.line(cross, fill=(74, 82, 88, 255), width=68, joint="curve")
    draw_dashed_line(draw, cross, fill=(242, 229, 164, 255), width=7, dash=24, gap=22)

    # Crosswalks / parking blocks.
    for x in range(206, 306, 20):
        draw.line((x, 285, x + 12, 350), fill=(232, 236, 224, 255), width=9)
    for i in range(5):
        rounded_rect(draw, (716 + i * 34, 708, 738 + i * 34, 764), 4, fill=(232, 236, 224, 255))

    # Houses.
    houses = [
        ((98, 92, 220, 196), (235, 145, 82, 255), (126, 74, 62, 255)),
        ((314, 704, 438, 810), (238, 202, 105, 255), (111, 93, 144, 255)),
        ((770, 620, 908, 720), (220, 111, 96, 255), (78, 112, 142, 255)),
    ]
    for rect, body, roof in houses:
        x0, y0, x1, y1 = rect
        rounded_rect(draw, rect, 16, fill=body, outline=(76, 72, 66, 255), width=5)
        draw.polygon([(x0 - 10, y0 + 18), ((x0 + x1) / 2, y0 - 42), (x1 + 10, y0 + 18)], fill=roof)
        rounded_rect(draw, (x0 + 44, y0 + 48, x0 + 78, y1 - 10), 7, fill=(88, 66, 54, 255))
        draw.rectangle((x1 - 46, y0 + 42, x1 - 18, y0 + 70), fill=(188, 229, 229, 255))

    # Trees.
    for x, y in [(76, 440), (112, 518), (374, 142), (916, 296), (912, 876), (542, 900), (72, 924)]:
        draw.rectangle((x - 9, y + 16, x + 9, y + 46), fill=(118, 79, 44, 255))
        draw.ellipse((x - 34, y - 28, x + 34, y + 38), fill=(54, 141, 76, 255), outline=(32, 96, 56, 255), width=4)
        draw.ellipse((x - 18, y - 44, x + 30, y + 10), fill=(73, 166, 85, 230))

    # Border stitching.
    draw.rounded_rectangle((18, 18, 1006, 1006), radius=36, outline=(235, 217, 163, 255), width=14)
    for i in range(34):
        t = 36 + i * 28
        draw.line((t, 16, t + 15, 30), fill=(166, 111, 76, 255), width=3)
        draw.line((t, 1008, t + 15, 994), fill=(166, 111, 76, 255), width=3)
        draw.line((16, t, 30, t + 15), fill=(166, 111, 76, 255), width=3)
        draw.line((1008, t, 994, t + 15), fill=(166, 111, 76, 255), width=3)

    image = image.filter(ImageFilter.UnsharpMask(radius=1, percent=105, threshold=3))
    image.save(SPRITES / "road_carpet_map_1024.png")


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
    image.save(SPRITES / "toy_car_top.png")


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
    image.save(SPRITES / "coin.png")


def main():
    SPRITES.mkdir(parents=True, exist_ok=True)
    make_map()
    make_car()
    make_coin()
    print(f"Generated assets in {SPRITES}")


if __name__ == "__main__":
    main()
