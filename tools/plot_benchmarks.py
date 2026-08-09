import csv
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
CSV_PATH = ROOT / "benchmark_results" / "results.csv"
RESULTS_DIR = ROOT / "results"

WIDTH = 1800
HEIGHT = 1050
LEFT = 170
PLOT_RIGHT = 1260
PANEL_LEFT = 1310
RIGHT = 60
TOP = 150
BOTTOM = 130

X_TICKS = [1, 10, 100, 1_000, 10_000, 100_000, 1_000_000]

COLORS = {
    "new_delete": (222, 70, 55),
    "std_allocator": (45, 130, 210),
    "object_pool": (35, 170, 95),
    "default_stl": (120, 120, 120),
    "pool_allocator": (145, 80, 190),
}

LABELS = {
    "new_delete": "new/delete",
    "std_allocator": "std::allocator",
    "object_pool": "ObjectPool",
    "default_stl": "default STL",
    "pool_allocator": "PoolAllocator",
}


def load_font(size, bold=False):
    candidates = [
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf" if bold else "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Helvetica.ttc",
        "/Library/Fonts/Arial.ttf",
        "arial.ttf",
    ]

    for path in candidates:
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            pass

    return ImageFont.load_default()


FONT_TITLE = load_font(38, bold=True)
FONT_SUBTITLE = load_font(22)
FONT_AXIS = load_font(24)
FONT_TICK = load_font(19)
FONT_PANEL_TITLE = load_font(24, bold=True)
FONT_PANEL = load_font(21)
FONT_PANEL_SMALL = load_font(18)


def read_rows():
    with CSV_PATH.open(newline="") as file:
        rows = list(csv.DictReader(file))

    for row in rows:
        row["operations"] = int(row["operations"])
        row["time_ms"] = float(row["time_ms"])
        row["speedup_vs_baseline"] = float(row["speedup_vs_baseline"])

    return rows


def fmt_ops(value):
    if value >= 1_000_000:
        return "1M"
    if value >= 1_000:
        return f"{value // 1_000}K"
    return str(value)


def fmt_ms(value):
    if value >= 100:
        return f"{value:.0f} ms"
    if value >= 10:
        return f"{value:.1f} ms"
    if value >= 1:
        return f"{value:.2f} ms"
    if value >= 0.001:
        return f"{value:.3f} ms"
    return f"{value:.1e} ms"


def fmt_axis_ms(value):
    if value >= 1:
        return f"{value:g}"
    if value >= 0.001:
        return f"{value:.3f}".rstrip("0").rstrip(".")
    return f"{value:.0e}"


def fmt_speedup(value):
    return f"{value:.2f}x"


def log_scale(value, source_min, source_max, target_min, target_max):
    value = max(value, 1e-12)
    source_min = max(source_min, 1e-12)
    source_max = max(source_max, source_min * 1.000001)
    return target_min + (math.log10(value) - math.log10(source_min)) * (target_max - target_min) / (math.log10(source_max) - math.log10(source_min))


def linear_scale(value, source_min, source_max, target_min, target_max):
    source_max = max(source_max, source_min + 1e-12)
    return target_min + (value - source_min) * (target_max - target_min) / (source_max - source_min)


def time_ticks(y_min, y_max):
    ticks = []
    low = math.floor(math.log10(max(y_min, 1e-9)))
    high = math.ceil(math.log10(max(y_max, 1e-9)))

    for exp in range(low, high + 1):
        tick = 10 ** exp
        if y_min <= tick <= y_max:
            ticks.append(tick)

    return ticks


def speedup_ticks(y_min, y_max):
    if y_max <= 2:
        step = 0.25
    elif y_max <= 6:
        step = 0.5
    elif y_max <= 12:
        step = 1.0
    else:
        step = 2.0

    ticks = []
    value = 0.0
    while value <= y_max + step:
        if value >= y_min:
            ticks.append(value)
        value += step
    return ticks


def draw_text(draw, xy, text, font, fill=(35, 35, 35), anchor=None):
    draw.text(xy, text, font=font, fill=fill, anchor=anchor)


def point_for(row, x_min, x_max, y_min, y_max, y_key, y_log):
    x = log_scale(row["operations"], x_min, x_max, LEFT, PLOT_RIGHT)
    if y_log:
        y = log_scale(row[y_key], y_min, y_max, HEIGHT - BOTTOM, TOP)
    else:
        y = linear_scale(row[y_key], y_min, y_max, HEIGHT - BOTTOM, TOP)
    return x, y


def draw_title(draw, title, subtitle):
    draw_text(draw, (WIDTH // 2, 48), title, FONT_TITLE, anchor="mm")
    draw_text(draw, (WIDTH // 2, 92), subtitle, FONT_SUBTITLE, fill=(85, 85, 85), anchor="mm")


def draw_axes(draw, x_min, x_max, y_min, y_max, y_ticks, y_log, y_label):
    plot_bottom = HEIGHT - BOTTOM

    draw.line((LEFT, plot_bottom, PLOT_RIGHT, plot_bottom), fill=(30, 30, 30), width=3)
    draw.line((LEFT, TOP, LEFT, plot_bottom), fill=(30, 30, 30), width=3)

    for tick in X_TICKS:
        x = log_scale(tick, x_min, x_max, LEFT, PLOT_RIGHT)
        draw.line((x, TOP, x, plot_bottom), fill=(232, 232, 232), width=1)
        draw_text(draw, (x, plot_bottom + 30), fmt_ops(tick), FONT_TICK, anchor="mm")

    for tick in y_ticks:
        if y_log:
            y = log_scale(tick, y_min, y_max, plot_bottom, TOP)
            label = fmt_axis_ms(tick)
        else:
            y = linear_scale(tick, y_min, y_max, plot_bottom, TOP)
            label = f"{tick:g}"

        draw.line((LEFT, y, PLOT_RIGHT, y), fill=(232, 232, 232), width=1)
        draw_text(draw, (LEFT - 14, y), label, FONT_TICK, anchor="rm")

    draw_text(draw, ((LEFT + PLOT_RIGHT) // 2, HEIGHT - 50), "operations (log scale)", FONT_AXIS, anchor="mm")
    draw_text(draw, (LEFT, TOP - 32), y_label, FONT_AXIS, anchor="lm")


def draw_series(draw, rows, allocators, x_min, x_max, y_min, y_max, y_key, y_log):
    for allocator in allocators:
        series = sorted(
            [row for row in rows if row["allocator"] == allocator],
            key=lambda row: row["operations"],
        )
        points = [point_for(row, x_min, x_max, y_min, y_max, y_key, y_log) for row in series]
        color = COLORS.get(allocator, (80, 80, 80))

        if len(points) > 1:
            draw.line(points, fill=color, width=5)

        for x, y in points:
            draw.ellipse((x - 7, y - 7, x + 7, y + 7), fill=color, outline=(255, 255, 255), width=2)


def rows_at(rows, operations):
    return sorted([row for row in rows if row["operations"] == operations], key=lambda row: row["allocator"])


def draw_side_panel(draw, rows, allocators, workload, y_key):
    panel_right = WIDTH - RIGHT
    panel_top = TOP
    panel_bottom = HEIGHT - BOTTOM

    draw.rounded_rectangle(
        (PANEL_LEFT, panel_top, panel_right, panel_bottom),
        radius=18,
        fill=(250, 250, 250),
        outline=(220, 220, 220),
        width=2,
    )

    draw_text(draw, (PANEL_LEFT + 28, panel_top + 34), "Legend", FONT_PANEL_TITLE, anchor="lm")

    y = panel_top + 76
    for allocator in allocators:
        color = COLORS.get(allocator, (80, 80, 80))
        draw.line((PANEL_LEFT + 30, y, PANEL_LEFT + 78, y), fill=color, width=6)
        draw.ellipse((PANEL_LEFT + 48, y - 8, PANEL_LEFT + 64, y + 8), fill=color)
        draw_text(draw, (PANEL_LEFT + 94, y), LABELS.get(allocator, allocator), FONT_PANEL, anchor="lm")
        y += 36

    y += 20
    draw_text(draw, (PANEL_LEFT + 28, y), "Exact values at 1M ops", FONT_PANEL_TITLE, anchor="lm")
    y += 42

    one_million = rows_at(rows, 1_000_000)
    for row in one_million:
        if row["allocator"] not in allocators:
            continue
        color = COLORS.get(row["allocator"], (80, 80, 80))
        label = LABELS.get(row["allocator"], row["allocator"])
        value = fmt_ms(row["time_ms"]) if y_key == "time_ms" else fmt_speedup(row["speedup_vs_baseline"])
        draw_text(draw, (PANEL_LEFT + 28, y), f"{label}:", FONT_PANEL, fill=color, anchor="lm")
        draw_text(draw, (panel_right - 28, y), value, FONT_PANEL, fill=color, anchor="rm")
        y += 34

    if y_key == "time_ms":
        new_row = next((row for row in one_million if row["allocator"] == "new_delete"), None)
        pool_row = next((row for row in one_million if row["allocator"] == "object_pool"), None)
        if new_row and pool_row:
            saved_ms = new_row["time_ms"] - pool_row["time_ms"]
            y += 22
            draw_text(draw, (PANEL_LEFT + 28, y), "Difference at 1M", FONT_PANEL_TITLE, anchor="lm")
            y += 38
            draw_text(draw, (PANEL_LEFT + 28, y), "new/delete - ObjectPool:", FONT_PANEL_SMALL, anchor="lm")
            draw_text(draw, (panel_right - 28, y), fmt_ms(saved_ms), FONT_PANEL, fill=(35, 170, 95), anchor="rm")
            y += 34
            draw_text(draw, (PANEL_LEFT + 28, y), "ObjectPool speedup:", FONT_PANEL_SMALL, anchor="lm")
            draw_text(draw, (panel_right - 28, y), fmt_speedup(new_row["time_ms"] / pool_row["time_ms"]), FONT_PANEL, fill=(35, 170, 95), anchor="rm")

    if y_key == "speedup_vs_baseline":
        optimized_allocator = "pool_allocator" if workload == "stl_order_book" else "object_pool"
        optimized_rows = [row for row in rows if row["allocator"] == optimized_allocator]
        if optimized_rows:
            best = max(optimized_rows, key=lambda row: row["speedup_vs_baseline"])
            y += 22
            draw_text(draw, (PANEL_LEFT + 28, y), f"Best {LABELS.get(optimized_allocator)} point", FONT_PANEL_TITLE, anchor="lm")
            y += 38
            draw_text(draw, (PANEL_LEFT + 28, y), f"{fmt_ops(best['operations'])} operations:", FONT_PANEL_SMALL, anchor="lm")
            draw_text(draw, (panel_right - 28, y), fmt_speedup(best["speedup_vs_baseline"]), FONT_PANEL, fill=COLORS.get(optimized_allocator, (80, 80, 80)), anchor="rm")

    draw_text(draw, (PANEL_LEFT + 28, panel_bottom - 34), f"workload: {workload}", FONT_PANEL_SMALL, fill=(90, 90, 90), anchor="lm")


def save_chart(rows, workload, y_key, title, y_label, filename, y_log=False):
    allocators = sorted({row["allocator"] for row in rows})
    if y_key == "speedup_vs_baseline":
        allocators = [allocator for allocator in allocators if allocator not in {"new_delete", "default_stl"}]

    if workload == "lifecycle" and "new_delete" in allocators:
        allocators.remove("new_delete")

    visible_rows = [row for row in rows if row["allocator"] in allocators]
    x_min = min(row["operations"] for row in rows)
    x_max = max(row["operations"] for row in rows)
    y_values = [row[y_key] for row in visible_rows]

    if y_key == "time_ms":
        y_min = max(min(y_values) * 0.7, 1e-6)
        y_max = max(y_values) * 1.4
        y_ticks = time_ticks(y_min, y_max)
        subtitle = "X-axis and Y-axis are log scale. Exact 1M values are shown on the right."
    else:
        y_min = min(0.0, min(y_values) * 0.9)
        y_max = max(1.1, max(y_values) * 1.2)
        y_ticks = speedup_ticks(y_min, y_max)
        subtitle = "X-axis is log scale. Y-axis is linear speedup over that workload's baseline."

    image = Image.new("RGB", (WIDTH, HEIGHT), "white")
    draw = ImageDraw.Draw(image)

    draw.rectangle((0, 0, WIDTH, HEIGHT), fill=(255, 255, 255))
    draw_axes(draw, x_min, x_max, y_min, y_max, y_ticks, y_log, y_label)
    draw_title(draw, title, subtitle)

    if y_key == "speedup_vs_baseline":
        y = linear_scale(1.0, y_min, y_max, HEIGHT - BOTTOM, TOP)
        draw.line((LEFT, y, PLOT_RIGHT, y), fill=(40, 40, 40), width=3)
        draw_text(draw, (PLOT_RIGHT - 8, y - 16), "1.0x baseline", FONT_TICK, anchor="ra")

    draw_series(draw, visible_rows, allocators, x_min, x_max, y_min, y_max, y_key, y_log)
    draw_side_panel(draw, visible_rows, allocators, workload, y_key)

    filepath = RESULTS_DIR / filename
    image.save(filepath, quality=95)
    return filepath


def combine_images(img1_path, img2_path, out_path):
    if not img1_path.exists() or not img2_path.exists():
        return
    img1 = Image.open(img1_path)
    img2 = Image.open(img2_path)
    combined = Image.new("RGB", (img1.width, img1.height + img2.height), "white")
    combined.paste(img1, (0, 0))
    combined.paste(img2, (0, img1.height))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    combined.save(out_path, quality=95)


def main():
    if not CSV_PATH.exists():
        raise FileNotFoundError(f"Missing benchmark CSV: {CSV_PATH}")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    rows = read_rows()

    for workload in sorted({row["workload"] for row in rows}):
        workload_rows = [row for row in rows if row["workload"] == workload]

        if workload == "lifecycle":
            chart_title = "stdallocator vs ObjectPoolAllocator(custom allocator)"
        elif workload == "stl_order_book":
            chart_title = "stdallocator vs PoolAllocator(custom allocator for STL)"
        else:
            chart_title = workload

        time_path = save_chart(
            workload_rows,
            workload,
            "time_ms",
            chart_title,
            "time in milliseconds (log scale)",
            f"{workload}_time_ms.png",
            y_log=True,
        )

        speedup_path = save_chart(
            workload_rows,
            workload,
            "speedup_vs_baseline",
            chart_title,
            "speedup multiplier",
            f"{workload}_speedup.png",
        )

        if workload in ["lifecycle", "stl_order_book"]:
            results2_dir = ROOT / "results2"
            combine_images(speedup_path, time_path, results2_dir / f"{workload}_combined.png")

    print(f"Saved PNG graphs to {RESULTS_DIR}")


if __name__ == "__main__":
    main()
