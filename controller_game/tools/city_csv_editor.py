from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import tkinter as tk
from tkinter import messagebox

try:
    from PIL import Image, ImageTk
    from PIL import ImageDraw
except ImportError:
    Image = None
    ImageTk = None
    ImageDraw = None


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CITY_PATH = ROOT / "assets" / "cities" / "demo_city.csv"
TILE_WORLD_SIZE = 256
CELL_SIZE = 84
PADDING = 48
SPRITES = ROOT / "assets" / "sprites"

if Image is not None:
    try:
        RESAMPLE_LANCZOS = Image.Resampling.LANCZOS
    except AttributeError:
        RESAMPLE_LANCZOS = Image.LANCZOS
else:
    RESAMPLE_LANCZOS = None


@dataclass
class CellStyle:
    fill: str
    outline: str
    short_label: str


def load_city_rows(path: Path) -> list[list[str]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    rows = [line.split(",") for line in lines]
    width = max((len(row) for row in rows), default=0)
    for row in rows:
      row.extend([""] * (width - len(row)))
    return rows


def save_city_rows(path: Path, rows: list[list[str]]) -> None:
    text = "\n".join(",".join(row) for row in rows) + "\n"
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)


def cell_tokens(cell: str) -> list[str]:
    return [token.strip() for token in cell.split("|") if token.strip()]


def token_name(token: str) -> str:
    base = token.split("@", 1)[0]
    base = base.split("*", 1)[0]
    return base


def sprite_filename_for_cell(cell: str) -> str | None:
    names = [token_name(token) for token in cell_tokens(cell)]
    sprite_map = {
        "house": "building_house.png",
        "shop": "building_shop.png",
        "school": "building_school_ai_01.png",
        "police": "building_police_station_ai_01.png",
        "fire_station": "building_fire_station.png",
        "library": "building_library.png",
        "tree:round": "prop_tree_round_ai_01.png",
        "tree:evergreen": "prop_evergreen.png",
        "bush": "prop_bush_cluster.png",
        "coin:star": "coin_star.png",
    }
    road_map = {
        "r_h": "road_horizontal_repeat.png",
        "r_v": "road_vertical_repeat.png",
        "r_x": "road_intersection_4way_crosswalks.png",
        "r_br": "road_curve_bottom_right.png",
        "r_bl": "road_curve_bottom_left.png",
        "r_tr": "road_curve_top_right.png",
        "r_tl": "road_curve_top_left.png",
    }

    for name in names:
        if name in sprite_map:
            return sprite_map[name]
    for name in names:
        if name in road_map:
            return road_map[name]
    return None


def road_token_for_cell(cell: str) -> str | None:
    for token in cell_tokens(cell):
        name = token_name(token)
        if name in {"r_h", "r_v", "r_x", "r_br", "r_bl", "r_tr", "r_tl"}:
            return name
    return None


def describe_cell(cell: str) -> CellStyle:
    tokens = cell_tokens(cell)
    names = [token_name(token) for token in tokens]
    has_road = any(name.startswith("r_") for name in names)
    has_building = any(name in {"house", "shop", "school", "police", "fire_station", "library"}
                       for name in names)
    has_coin = "coin:star" in names
    has_spawn = "spawn:player" in names
    has_kraken = any(name.startswith("kraken:") for name in names)
    has_prop = any(name in {"tree:round", "tree:evergreen", "bush"} for name in names)

    if has_spawn:
        return CellStyle("#8ecae6", "#3d5a80", "SP")
    if has_kraken:
        return CellStyle("#ffc8dd", "#9d4edd", "KR")
    if has_building and has_road:
        return CellStyle("#ddb892", "#9c6644", "B+R")
    if has_building:
        return CellStyle("#f4a261", "#9c6644", "BLD")
    if has_road and has_coin:
        return CellStyle("#adb5bd", "#495057", "R+$")
    if has_road:
        return CellStyle("#ced4da", "#495057", "RD")
    if has_coin:
        return CellStyle("#ffe066", "#b08900", "$")
    if has_prop:
        return CellStyle("#95d5b2", "#2d6a4f", "PROP")
    if not names:
        return CellStyle("#f8f9fa", "#adb5bd", "")
    return CellStyle("#e9ecef", "#6c757d", "OBJ")


def first_label_lines(cell: str) -> list[str]:
    tokens = cell_tokens(cell)
    if not tokens:
        return []

    display = []
    for token in tokens[:2]:
        name = token_name(token)
        label = (
            name.replace("fire_station", "fire")
            .replace("spawn:player", "spawn")
            .replace("tree:evergreen", "evergreen")
            .replace("tree:round", "round tree")
            .replace("coin:star", "coin")
            .replace("kraken:road", "kraken")
            .replace("kraken:pop", "kraken")
        )
        display.append(label[:12])
    return display


class CityEditorApp:
    def __init__(self, root: tk.Tk, city_path: Path):
        self.root = root
        self.city_path = city_path
        self.rows = load_city_rows(city_path)
        self.selected_row = 0
        self.selected_col = 0
        self.dirty = False
        self.cell_items: dict[tuple[int, int], int] = {}
        self.sprite_cache: dict[str, object] = {}
        self.editor_modified = False

        self.root.title(f"Town CSV Editor - {self.city_path.name}")
        self.root.geometry("1500x900")

        self.status_var = tk.StringVar()
        self.path_var = tk.StringVar(value=str(self.city_path))
        self.coord_var = tk.StringVar()
        self.world_var = tk.StringVar()
        self.summary_var = tk.StringVar()

        self.build_ui()
        self.bind_shortcuts()
        self.redraw_grid()
        self.select_cell(0, 0)

    @property
    def row_count(self) -> int:
        return len(self.rows)

    @property
    def col_count(self) -> int:
        return len(self.rows[0]) if self.rows else 0

    def build_ui(self) -> None:
        root_frame = tk.Frame(self.root)
        root_frame.pack(fill=tk.BOTH, expand=True)

        left = tk.Frame(root_frame)
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        right = tk.Frame(root_frame, width=360, padx=12, pady=12)
        right.pack(side=tk.RIGHT, fill=tk.Y)

        top_bar = tk.Frame(left, padx=8, pady=8)
        top_bar.pack(fill=tk.X)
        tk.Label(top_bar, text="CSV:").pack(side=tk.LEFT)
        tk.Label(top_bar, textvariable=self.path_var, anchor="w").pack(side=tk.LEFT, fill=tk.X, expand=True)

        canvas_frame = tk.Frame(left)
        canvas_frame.pack(fill=tk.BOTH, expand=True)

        self.canvas = tk.Canvas(canvas_frame, background="#ffffff")
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        yscroll = tk.Scrollbar(canvas_frame, orient=tk.VERTICAL, command=self.canvas.yview)
        yscroll.pack(side=tk.RIGHT, fill=tk.Y)
        xscroll = tk.Scrollbar(left, orient=tk.HORIZONTAL, command=self.canvas.xview)
        xscroll.pack(fill=tk.X)
        self.canvas.configure(yscrollcommand=yscroll.set, xscrollcommand=xscroll.set)
        self.canvas.bind("<Button-1>", self.on_canvas_click)

        tk.Label(right, text="Selected Cell", font=("Segoe UI", 12, "bold")).pack(anchor="w")
        tk.Label(right, textvariable=self.coord_var, anchor="w").pack(anchor="w", pady=(6, 0))
        tk.Label(right, textvariable=self.world_var, anchor="w").pack(anchor="w", pady=(2, 0))
        tk.Label(right, textvariable=self.summary_var, anchor="w", justify=tk.LEFT, wraplength=320).pack(
            anchor="w", pady=(8, 12)
        )

        tk.Label(right, text="Cell Contents").pack(anchor="w")
        self.editor = tk.Text(right, height=8, width=40, wrap="word")
        self.editor.pack(fill=tk.X, pady=(4, 8))
        self.editor.bind("<<Modified>>", self.on_editor_modified)

        button_row = tk.Frame(right)
        button_row.pack(fill=tk.X, pady=(0, 12))
        tk.Button(button_row, text="Apply To Cell", command=self.apply_editor_to_cell).pack(side=tk.LEFT)
        tk.Button(button_row, text="Clear Cell", command=self.clear_selected_cell).pack(side=tk.LEFT, padx=(8, 0))

        save_row = tk.Frame(right)
        save_row.pack(fill=tk.X, pady=(0, 16))
        tk.Button(save_row, text="Save", command=self.save).pack(side=tk.LEFT)
        tk.Button(save_row, text="Reload", command=self.reload).pack(side=tk.LEFT, padx=(8, 0))

        tk.Label(right, text="Quick Tips", font=("Segoe UI", 11, "bold")).pack(anchor="w")
        tips = (
            "- Click a tile to inspect/edit it.\n"
            "- Row/col are 1-based and match the spreadsheet view.\n"
            "- World center is based on 256 px tiles.\n"
            "- Use arrow keys to move selection.\n"
            "- Ctrl+S saves, Ctrl+R reloads.\n"
            "- Edit the raw pipe-separated cell text directly."
        )
        tk.Label(right, text=tips, justify=tk.LEFT, wraplength=320).pack(anchor="w", pady=(6, 12))

        tk.Label(right, text="Legend", font=("Segoe UI", 11, "bold")).pack(anchor="w")
        legend_items = [
            ("RD", "#ced4da", "Road"),
            ("R+$", "#adb5bd", "Road with coin"),
            ("BLD", "#f4a261", "Building"),
            ("B+R", "#ddb892", "Building on road tile"),
            ("PROP", "#95d5b2", "Trees / bushes"),
            ("KR", "#ffc8dd", "Kraken"),
            ("SP", "#8ecae6", "Spawn"),
        ]
        for short, color, label in legend_items:
            row = tk.Frame(right)
            row.pack(anchor="w", pady=1)
            swatch = tk.Canvas(row, width=18, height=18, highlightthickness=1, highlightbackground="#888")
            swatch.create_rectangle(0, 0, 18, 18, fill=color, outline=color)
            swatch.pack(side=tk.LEFT)
            tk.Label(row, text=f"{short}  {label}").pack(side=tk.LEFT, padx=(8, 0))

        tk.Label(self.root, textvariable=self.status_var, anchor="w", bd=1, relief=tk.SUNKEN).pack(
            side=tk.BOTTOM, fill=tk.X
        )

    def bind_shortcuts(self) -> None:
        self.root.bind("<Control-s>", lambda _event: self.save())
        self.root.bind("<Control-r>", lambda _event: self.reload())
        self.root.bind("<Up>", lambda _event: self.move_selection(-1, 0))
        self.root.bind("<Down>", lambda _event: self.move_selection(1, 0))
        self.root.bind("<Left>", lambda _event: self.move_selection(0, -1))
        self.root.bind("<Right>", lambda _event: self.move_selection(0, 1))

    def redraw_grid(self) -> None:
        self.canvas.delete("all")
        self.cell_items.clear()

        width = PADDING + self.col_count * CELL_SIZE + 1
        height = PADDING + self.row_count * CELL_SIZE + 1
        self.canvas.configure(scrollregion=(0, 0, width, height))

        for col in range(self.col_count):
            x = PADDING + col * CELL_SIZE + CELL_SIZE * 0.5
            self.canvas.create_text(x, PADDING * 0.45, text=str(col + 1), font=("Segoe UI", 10, "bold"))

        for row in range(self.row_count):
            y = PADDING + row * CELL_SIZE + CELL_SIZE * 0.5
            self.canvas.create_text(PADDING * 0.45, y, text=str(row + 1), font=("Segoe UI", 10, "bold"))

        for row in range(self.row_count):
            for col in range(self.col_count):
                self.draw_cell(row, col)

    def draw_cell(self, row: int, col: int) -> None:
        cell = self.rows[row][col]
        style = describe_cell(cell)
        x0 = PADDING + col * CELL_SIZE
        y0 = PADDING + row * CELL_SIZE
        x1 = x0 + CELL_SIZE
        y1 = y0 + CELL_SIZE
        outline_width = 3 if (row, col) == (self.selected_row, self.selected_col) else 1
        outline = "#d62828" if (row, col) == (self.selected_row, self.selected_col) else style.outline
        self.canvas.create_rectangle(x0, y0, x1, y1, fill=style.fill, outline=outline, width=outline_width)
        sprite = self.get_cell_sprite(cell)
        if sprite is not None:
            self.canvas.create_image(x0 + CELL_SIZE * 0.5, y0 + CELL_SIZE * 0.55, image=sprite)
        self.canvas.create_text(
            x0 + 6, y0 + 6, text=f"{row + 1},{col + 1}", anchor="nw", font=("Consolas", 8), fill="#343a40"
        )
        if style.short_label:
            self.canvas.create_text(
                x0 + CELL_SIZE - 6,
                y0 + 6,
                text=style.short_label,
                anchor="ne",
                font=("Segoe UI", 8, "bold"),
                fill="#343a40",
            )
        lines = first_label_lines(cell)
        if lines:
            text_box_top = y0 + CELL_SIZE * 0.45 - 10
            text_box_bottom = text_box_top + 10 + len(lines) * 14
            self.canvas.create_rectangle(
                x0 + 8,
                text_box_top,
                x1 - 8,
                text_box_bottom,
                fill="#ffffff",
                outline="",
                stipple="gray25",
            )
        for index, line in enumerate(lines):
            self.canvas.create_text(
                x0 + CELL_SIZE * 0.5,
                y0 + CELL_SIZE * 0.45 + index * 14,
                text=line,
                anchor="center",
                font=("Segoe UI", 9),
                fill="#212529",
            )

    def on_canvas_click(self, event: tk.Event) -> None:
        x = self.canvas.canvasx(event.x) - PADDING
        y = self.canvas.canvasy(event.y) - PADDING
        if x < 0 or y < 0:
            return
        col = int(x // CELL_SIZE)
        row = int(y // CELL_SIZE)
        if 0 <= row < self.row_count and 0 <= col < self.col_count:
            self.select_cell(row, col)

    def on_editor_modified(self, _event: tk.Event) -> None:
        if self.editor.edit_modified():
            self.editor_modified = True
            self.dirty = True
            self.status_var.set("Edited current cell text. Save to write changes to disk.")
            self.editor.edit_modified(False)

    def get_cell_sprite(self, cell: str):
        if Image is None or ImageTk is None:
            return None
        road_token = road_token_for_cell(cell)
        if road_token:
            cache_key = f"road:{cell}"
            if cache_key in self.sprite_cache:
                return self.sprite_cache[cache_key]
            photo = self.make_road_preview(cell, road_token)
            self.sprite_cache[cache_key] = photo
            return photo

        filename = sprite_filename_for_cell(cell)
        if not filename:
            return None
        if filename in self.sprite_cache:
            return self.sprite_cache[filename]

        sprite_path = SPRITES / filename
        if not sprite_path.exists():
            return None

        image = Image.open(sprite_path).convert("RGBA")
        image.thumbnail((CELL_SIZE - 16, CELL_SIZE - 16), RESAMPLE_LANCZOS)
        photo = ImageTk.PhotoImage(image)
        self.sprite_cache[filename] = photo
        return photo

    def make_road_preview(self, cell: str, road_token: str):
        size = CELL_SIZE - 16
        image = Image.new("RGBA", (size, size), (68, 142, 134, 255))
        draw = ImageDraw.Draw(image)

        road_fill = (78, 86, 96, 255)
        lane_fill = (210, 218, 226, 255)
        road_width = int(size * 0.36)
        cx = size // 2
        cy = size // 2
        half = road_width // 2

        def draw_horizontal():
            draw.rounded_rectangle((0, cy - half, size, cy + half), radius=half // 2, fill=road_fill)
            draw.rounded_rectangle((0, cy - 3, size, cy + 3), radius=3, fill=lane_fill)

        def draw_vertical():
            draw.rounded_rectangle((cx - half, 0, cx + half, size), radius=half // 2, fill=road_fill)
            draw.rounded_rectangle((cx - 3, 0, cx + 3, size), radius=3, fill=lane_fill)

        def draw_curve(corner_x: int, corner_y: int, start_deg: int, end_deg: int):
            outer = [corner_x - road_width, corner_y - road_width, corner_x + road_width, corner_y + road_width]
            inner_margin = int(road_width * 0.42)
            inner = [
                outer[0] + inner_margin,
                outer[1] + inner_margin,
                outer[2] - inner_margin,
                outer[3] - inner_margin,
            ]
            draw.pieslice(outer, start=start_deg, end=end_deg, fill=road_fill)
            draw.pieslice(inner, start=start_deg, end=end_deg, fill=(68, 142, 134, 255))

        if road_token == "r_h":
            draw_horizontal()
        elif road_token == "r_v":
            draw_vertical()
        elif road_token == "r_x":
            draw_horizontal()
            draw_vertical()
        elif road_token == "r_br":
            draw_curve(size, size, 180, 270)
        elif road_token == "r_bl":
            draw_curve(0, size, 270, 360)
        elif road_token == "r_tr":
            draw_curve(size, 0, 90, 180)
        elif road_token == "r_tl":
            draw_curve(0, 0, 0, 90)

        names = [token_name(token) for token in cell_tokens(cell)]
        if "coin:star" in names:
            draw.ellipse((size - 20, 4, size - 4, 20), fill=(255, 214, 10, 255), outline=(176, 128, 0, 255))
        if "spawn:player" in names:
            draw.ellipse((4, 4, 20, 20), fill=(69, 123, 157, 255), outline=(29, 53, 87, 255))
        if any(name.startswith("kraken:") for name in names):
            draw.ellipse((4, size - 20, 20, size - 4), fill=(181, 23, 158, 255), outline=(86, 11, 173, 255))

        return ImageTk.PhotoImage(image)

    def get_editor_value(self) -> str:
        return self.editor.get("1.0", tk.END).strip()

    def apply_current_editor_if_needed(self) -> bool:
        value = self.get_editor_value()
        if value == self.rows[self.selected_row][self.selected_col]:
            self.editor_modified = False
            return False

        self.rows[self.selected_row][self.selected_col] = value
        self.editor_modified = False
        self.dirty = True
        return True

    def select_cell(self, row: int, col: int) -> None:
        changed_current = self.apply_current_editor_if_needed()
        self.selected_row = row
        self.selected_col = col
        self.redraw_grid()
        cell = self.rows[row][col]
        self.editor.delete("1.0", tk.END)
        self.editor.insert("1.0", cell)
        self.editor.edit_modified(False)
        self.editor_modified = False
        self.coord_var.set(f"Row {row + 1}, Col {col + 1}")
        center_x = col * TILE_WORLD_SIZE + TILE_WORLD_SIZE // 2
        center_y = row * TILE_WORLD_SIZE + TILE_WORLD_SIZE // 2
        self.world_var.set(f"World center: ({center_x}, {center_y}) px")
        tokens = cell_tokens(cell)
        self.summary_var.set(
            "Tokens:\n" + ("\n".join(tokens) if tokens else "(empty grass tile)")
        )
        self.status_var.set("Applied pending edit and selected new cell." if changed_current else "Selected cell updated.")

    def move_selection(self, row_delta: int, col_delta: int) -> None:
        row = min(max(self.selected_row + row_delta, 0), self.row_count - 1)
        col = min(max(self.selected_col + col_delta, 0), self.col_count - 1)
        self.select_cell(row, col)

    def apply_editor_to_cell(self) -> None:
        changed = self.apply_current_editor_if_needed()
        self.redraw_grid()
        self.select_cell(self.selected_row, self.selected_col)
        self.status_var.set("Applied editor text to the selected cell." if changed else "Cell text already matched the editor.")

    def clear_selected_cell(self) -> None:
        self.editor.delete("1.0", tk.END)
        self.apply_editor_to_cell()

    def save(self) -> None:
        self.apply_current_editor_if_needed()
        self.redraw_grid()
        try:
            save_city_rows(self.city_path, self.rows)
        except OSError as exc:
            messagebox.showerror("Save failed", f"Could not save city CSV:\n{exc}")
            self.status_var.set(f"Save failed: {exc}")
            return
        self.dirty = False
        self.editor_modified = False
        self.status_var.set(f"Saved {self.city_path}")

    def reload(self) -> None:
        if self.dirty or self.get_editor_value() != self.rows[self.selected_row][self.selected_col]:
            confirmed = messagebox.askyesno(
                "Reload city CSV",
                "Reload from disk and discard unsaved changes?",
            )
            if not confirmed:
                return
        self.rows = load_city_rows(self.city_path)
        self.dirty = False
        self.editor_modified = False
        self.redraw_grid()
        self.select_cell(min(self.selected_row, self.row_count - 1), min(self.selected_col, self.col_count - 1))
        self.status_var.set(f"Reloaded {self.city_path}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Simple visual editor for the town CSV map.")
    parser.add_argument(
        "city_csv",
        nargs="?",
        default=str(DEFAULT_CITY_PATH),
        help="Path to the city CSV file. Defaults to controller_game/assets/cities/demo_city.csv",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    city_path = Path(args.city_csv).resolve()
    if not city_path.exists():
        raise SystemExit(f"City CSV not found: {city_path}")

    root = tk.Tk()
    app = CityEditorApp(root, city_path)
    app.status_var.set("Ready. Click a tile to edit it.")
    root.mainloop()


if __name__ == "__main__":
    main()
