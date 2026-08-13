"""Generate Peek's deterministic application icon assets."""

from pathlib import Path
from math import pi, sin

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
RESOURCES = ROOT / "resources"
DOC_IMAGES = ROOT / "docs" / "images"


def make_icon(size: int = 1024) -> Image.Image:
    scale = size / 256.0

    def s(value: float) -> int:
        return round(value * scale)

    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (s(8), s(8), s(248), s(248)), radius=s(54), fill=255
    )

    background = Image.new("RGBA", (size, size))
    pixels = background.load()
    for y in range(size):
        for x in range(size):
            t = (x + y) / max(1, 2 * size - 2)
            pixels[x, y] = (
                round(8 + 16 * t),
                round(22 + 37 * t),
                round(48 + 83 * t),
                255,
            )
    image.paste(background, (0, 0), mask)
    draw = ImageDraw.Draw(image)

    # Focus brackets reinforce the inspection metaphor without adding tiny text.
    cyan = (34, 211, 238, 255)
    stroke = s(8)
    draw.line((s(32), s(79), s(32), s(39), s(72), s(39)), fill=cyan, width=stroke)
    draw.line((s(224), s(177), s(224), s(217), s(184), s(217)), fill=cyan, width=stroke)

    # A compact eye/aperture mark remains legible in the 16 px tray icon.
    top = []
    bottom = []
    for index in range(65):
        t = index / 64
        x = 52 + 152 * t
        offset = 49 * sin(pi * t)
        top.append((s(x), s(128 - offset)))
        bottom.append((s(x), s(128 + offset)))
    draw.polygon(top + list(reversed(bottom)), fill=(246, 249, 255, 255))
    draw.ellipse((s(91), s(91), s(165), s(165)), fill=(59, 130, 246, 255))
    draw.ellipse((s(110), s(110), s(146), s(146)), fill=(8, 23, 48, 255))
    draw.ellipse((s(116), s(113), s(127), s(124)), fill=(255, 255, 255, 230))
    return image


def main() -> None:
    RESOURCES.mkdir(parents=True, exist_ok=True)
    DOC_IMAGES.mkdir(parents=True, exist_ok=True)

    master = make_icon()
    preview = master.resize((512, 512), Image.Resampling.LANCZOS)
    preview.save(DOC_IMAGES / "peek-icon.png", optimize=True)

    ico_source = master.resize((256, 256), Image.Resampling.LANCZOS)
    ico_source.save(
        RESOURCES / "peek.ico",
        format="ICO",
        sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40),
               (48, 48), (64, 64), (128, 128), (256, 256)],
    )


if __name__ == "__main__":
    main()
