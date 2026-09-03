#!/usr/bin/env python3
"""Turn the frames written by tools/preview into reviewable PNGs.

The device draws text with LVGL's Montserrat; this stand-in uses whatever
sans-serif is installed, so judge composition and legibility zones here, not
letterforms.

    make -C tools preview && mkdir -p out && tools/preview out && \
        python3 tools/contact_sheet.py out
"""
import sys
import os
from PIL import Image, ImageDraw, ImageFont

SIZE = 466
CENTER = SIZE // 2

FONT_CANDIDATES = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
]


def load_font(size):
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def draw_tracked(draw, text, font, cy, opacity, tracking=5):
    """Centred, letter-spaced text at the given opacity."""
    if not text or opacity <= 0.002:
        return
    widths = [draw.textlength(ch, font=font) for ch in text]
    total = sum(widths) + tracking * (len(text) - 1)
    x = CENTER - total / 2
    value = int(233 * opacity)
    colour = (value, value, int(value * 0.96))
    ascent, descent = font.getmetrics()
    y = cy - (ascent + descent) / 2
    for ch, w in zip(text, widths):
        draw.text((x, y), ch, font=font, fill=colour)
        x += w + tracking


def render(out_dir, name, label, state, text, text_opacity, progress,
           progress_opacity):
    image = Image.open(os.path.join(out_dir, name)).convert("RGB")
    draw = ImageDraw.Draw(image)

    lines = [line for line in text.split("|") if line]
    if lines:
        single_word = len(lines) == 1 and " " not in lines[0]
        font = load_font(28 if single_word else 24)
        spacing = 42
        top = CENTER - spacing * (len(lines) - 1) / 2
        for index, line in enumerate(lines):
            draw_tracked(draw, line, font, top + index * spacing, text_opacity)

    if progress_opacity > 0.01 and progress > 0.0:
        value = int(46 * progress_opacity)
        box = [CENTER - 222, CENTER - 222, CENTER + 222, CENTER + 222]
        draw.arc(box, start=-90, end=-90 + 360 * progress,
                 fill=(value, value, value), width=2)

    out_path = os.path.join(out_dir, name.replace(".ppm", ".png"))
    image.save(out_path)
    return image, label, state


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "out"
    frames = []
    with open(os.path.join(out_dir, "index.txt")) as handle:
        for line in handle:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 7:
                continue
            name, label, state, text, opacity, progress, progress_opacity = parts
            frames.append(render(out_dir, name, label, state, text,
                                 float(opacity), float(progress),
                                 float(progress_opacity)))

    columns = 6
    rows = (len(frames) + columns - 1) // columns
    tile = 233
    pad = 10
    caption = 20
    sheet = Image.new(
        "RGB",
        (columns * (tile + pad) + pad, rows * (tile + pad + caption) + pad),
        (16, 16, 18))
    draw = ImageDraw.Draw(sheet)
    small = load_font(13)
    for index, (image, label, state) in enumerate(frames):
        col, row = index % columns, index // columns
        x = pad + col * (tile + pad)
        y = pad + row * (tile + pad + caption)
        sheet.paste(image.resize((tile, tile), Image.LANCZOS), (x, y))
        draw.text((x, y + tile + 3), "%s  ·  %s" % (label, state),
                  font=small, fill=(150, 150, 155))
    sheet_path = os.path.join(out_dir, "contact-sheet.png")
    sheet.save(sheet_path)
    print("wrote", sheet_path)


if __name__ == "__main__":
    main()
