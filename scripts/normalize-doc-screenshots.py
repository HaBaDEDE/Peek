"""Normalize Peek documentation screenshots with the Windows system cursor.

The script rebuilds the small tab strip consistently and composites the real
Windows Aero arrow from the local system cursor file.  It is safe to rerun.
"""

from pathlib import Path
import struct

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
IMAGES = ROOT / "docs" / "images"
CURSOR_PATH = Path("C:/Windows/Cursors/aero_arrow.cur")
FONT_REGULAR = Path("C:/Windows/Fonts/segoeui.ttf")
FONT_CJK = Path("C:/Windows/Fonts/msyh.ttc")


def load_cursor(size: int = 32) -> Image.Image:
    """Read one BGRA frame from a Windows .cur file, preserving its alpha."""
    data = CURSOR_PATH.read_bytes()
    reserved, cursor_type, count = struct.unpack_from("<HHH", data, 0)
    if reserved != 0 or cursor_type != 2:
        raise ValueError(f"Unsupported cursor file: {CURSOR_PATH}")

    selected = None
    for index in range(count):
        entry = struct.unpack_from("<BBBBHHII", data, 6 + index * 16)
        width = entry[0] or 256
        height = entry[1] or 256
        if width == size and height == size:
            selected = entry
            break
    if selected is None:
        raise ValueError(f"{CURSOR_PATH} does not contain a {size}px cursor")

    image_offset = selected[-1]
    header_size, width, doubled_height, planes, bits_per_pixel, compression = (
        struct.unpack_from("<IiiHHI", data, image_offset)
    )
    height = doubled_height // 2
    if header_size < 40 or planes != 1 or bits_per_pixel != 32 or compression != 0:
        raise ValueError("Expected an uncompressed 32-bit Windows cursor frame")

    pixel_offset = image_offset + header_size
    pixel_bytes = data[pixel_offset:pixel_offset + width * height * 4]
    cursor = Image.frombytes("RGBA", (width, height), pixel_bytes, "raw", "BGRA")
    return cursor.transpose(Image.Transpose.FLIP_TOP_BOTTOM)


def normalize_tabs(
    image_name: str,
    labels: tuple[str, str, str],
    active_index: int,
    cursor_x: int,
) -> None:
    path = IMAGES / image_name
    image = Image.open(path).convert("RGBA")
    draw = ImageDraw.Draw(image)

    # Repaint only the tab area; the rest remains a real application capture.
    draw.rectangle((20, 30, 304, 87), fill=(248, 249, 251, 255))
    centers = (61, 156, 252)
    font_path = FONT_CJK if any(any(ord(char) > 127 for char in label) for label in labels) else FONT_REGULAR
    tab_font = ImageFont.truetype(str(font_path), 14)
    for index, (center, label) in enumerate(zip(centers, labels)):
        color = (37, 99, 235, 255) if index == active_index else (55, 65, 81, 255)
        draw.text((center, 59), label, font=tab_font, fill=color, anchor="mm")

    active_center = centers[active_index]
    draw.rectangle((active_center - 28, 84, active_center + 28, 87),
                   fill=(47, 120, 232, 255))

    # The cursor is the actual Windows Aero arrow, positioned beside the label
    # so the control text remains readable.
    image.alpha_composite(load_cursor(), (cursor_x, 38))
    image.convert("RGB").save(path, optimize=True)


def main() -> None:
    english = ("Ghost", "Unlock", "Focus")
    chinese = ("闪现", "解锁", "探查")
    positions = (83, 182, 273)
    for index, name in enumerate(("peek-ghost-en.png", "peek-unlock-en.png", "peek-focus-en.png")):
        normalize_tabs(name, english, index, positions[index])
    for index, name in enumerate(("peek-ghost.png", "peek-unlock.png", "peek-focus.png")):
        normalize_tabs(name, chinese, index, positions[index])


if __name__ == "__main__":
    main()
