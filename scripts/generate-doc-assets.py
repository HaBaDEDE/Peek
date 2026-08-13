"""Build README and social-preview images from real Peek screenshots."""

from argparse import ArgumentParser
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageOps


ROOT = Path(__file__).resolve().parents[1]
IMAGES = ROOT / "docs" / "images"
FONT_REGULAR = Path("C:/Windows/Fonts/segoeui.ttf")
FONT_SEMIBOLD = Path("C:/Windows/Fonts/seguisb.ttf")


def font(size: int, semibold: bool = False) -> ImageFont.FreeTypeFont:
    path = FONT_SEMIBOLD if semibold else FONT_REGULAR
    return ImageFont.truetype(str(path), size)


def paste_window(canvas: Image.Image, screenshot: Image.Image, box: tuple[int, int, int, int]) -> None:
    x, y, width, height = box
    fitted = ImageOps.contain(screenshot.convert("RGB"), (width, height), Image.Resampling.LANCZOS)
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    shadow_draw.rounded_rectangle(
        (x + 8, y + 12, x + fitted.width + 8, y + fitted.height + 12),
        radius=16,
        fill=(1, 12, 30, 95),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(14))
    canvas.alpha_composite(shadow)
    canvas.alpha_composite(fitted.convert("RGBA"), (x, y))


def load(name: str) -> Image.Image:
    return Image.open(IMAGES / name).convert("RGBA")


def make_overview() -> None:
    width, height = 1440, 760
    canvas = Image.new("RGBA", (width, height), (243, 247, 253, 255))
    draw = ImageDraw.Draw(canvas)
    draw.text((width // 2, 44), "Three questions. One tiny Windows tool.",
              font=font(42, True), fill=(15, 32, 62), anchor="ma")
    draw.text((width // 2, 101), "Ghost  ·  Unlock  ·  Focus",
              font=font(25), fill=(66, 91, 128), anchor="ma")

    cards = [
        ("peek-ghost-en.png", "Ghost", "What just flashed past?"),
        ("peek-unlock-en.png", "Unlock", "Who is using this file?"),
        ("peek-focus-en.png", "Focus", "What UI is under the pointer?"),
    ]
    for index, (image_name, title, subtitle) in enumerate(cards):
        x = 72 + index * 455
        draw.rounded_rectangle((x, 151, x + 388, 706), radius=24, fill=(255, 255, 255),
                               outline=(213, 224, 240), width=2)
        draw.text((x + 28, 177), title, font=font(27, True), fill=(37, 99, 235))
        draw.text((x + 28, 217), subtitle, font=font(18), fill=(73, 88, 112))
        screenshot = load(image_name)
        fitted = ImageOps.contain(screenshot, (332, 448), Image.Resampling.LANCZOS)
        canvas.alpha_composite(fitted, (x + (388 - fitted.width) // 2, 252))
    canvas.convert("RGB").save(IMAGES / "peek-overview.png", optimize=True)


def make_demo_gif() -> None:
    width, height = 960, 600
    features = [
        ("peek-ghost-en.png", "Ghost", "What just flashed past?", "Ctrl + Shift + G"),
        ("peek-unlock-en.png", "Unlock", "Who is using this file?", "Drop a file or choose one"),
        ("peek-focus-en.png", "Focus", "What UI is under the pointer?", "Ctrl + Shift + F"),
    ]
    icon = load("peek-icon.png").resize((92, 92), Image.Resampling.LANCZOS)
    frames: list[Image.Image] = []
    for screenshot_name, title, question, action in features:
        frame = Image.new("RGBA", (width, height), (9, 24, 51, 255))
        draw = ImageDraw.Draw(frame)
        for x in range(width):
            t = x / (width - 1)
            draw.line((x, 0, x, height), fill=(9 + round(12 * t), 24 + round(26 * t), 51 + round(60 * t), 255))
        frame.alpha_composite(icon, (60, 68))
        draw.text((60, 193), "Peek", font=font(64, True), fill=(246, 249, 255))
        draw.text((60, 282), title, font=font(35, True), fill=(56, 189, 248))
        draw.multiline_text((60, 340), question, font=font(26), fill=(215, 227, 246), spacing=8)
        draw.rounded_rectangle((60, 448, 430, 508), radius=14, fill=(37, 99, 235))
        draw.text((245, 478), action, font=font(20, True), fill=(255, 255, 255), anchor="mm")
        draw.text((60, 548), "Native Win32  ·  Offline  ·  Single EXE", font=font(17), fill=(138, 164, 201))
        paste_window(frame, load(screenshot_name), (563, 40, 356, 520))
        frames.append(frame.convert("P", palette=Image.Palette.ADAPTIVE, colors=128))

    frames[0].save(
        IMAGES / "peek-demo.gif",
        save_all=True,
        append_images=frames[1:],
        duration=[1600, 1600, 1800],
        loop=0,
        optimize=True,
        disposal=2,
    )


def make_social_preview(_background_path: Path | None) -> None:
    width, height = 1280, 640
    background = Image.new("RGBA", (width, height), (245, 247, 250, 255))
    draw = ImageDraw.Draw(background)

    # Flat colors and real screenshots keep the design consistent with the app.
    draw.rectangle((0, 0, 354, height), fill=(12, 20, 34, 255))
    draw.rectangle((354, 0, 362, height), fill=(37, 99, 235, 255))

    icon = load("peek-icon.png").resize((76, 76), Image.Resampling.LANCZOS)
    background.alpha_composite(icon, (48, 46))
    draw.text((144, 49), "Peek", font=font(47, True), fill=(248, 250, 252))
    draw.text((48, 151), "A tiny native Windows utility.",
              font=font(21), fill=(187, 200, 219))

    features = [
        ("Ghost", "What just flashed past?"),
        ("Unlock", "Who is using this file?"),
        ("Focus", "What UI is under the pointer?"),
    ]
    y = 218
    for title, question in features:
        draw.rectangle((48, y + 7, 54, y + 37), fill=(59, 130, 246))
        draw.text((70, y), title, font=font(24, True), fill=(248, 250, 252))
        draw.text((70, y + 34), question, font=font(16), fill=(157, 174, 199))
        y += 92

    draw.line((48, 529, 306, 529), fill=(48, 62, 82), width=1)
    draw.text((48, 554), "Win32  ·  Offline  ·  Portable",
              font=font(17), fill=(151, 168, 192))
    draw.text((48, 585), "Windows 10 / 11  ·  x64",
              font=font(17), fill=(151, 168, 192))

    draw.text((402, 38), "Three questions. One window.",
              font=font(34, True), fill=(24, 35, 52))
    draw.text((402, 81), "Native Win32 utility for Windows 10 and 11.",
              font=font(18), fill=(88, 101, 120))

    cards = [
        ("peek-ghost-en.png", "Ghost"),
        ("peek-unlock-en.png", "Unlock"),
        ("peek-focus-en.png", "Focus"),
    ]
    card_y = 124
    card_width = 268
    card_height = 462
    for index, (image_name, title) in enumerate(cards):
        x = 398 + index * 286
        draw.rounded_rectangle(
            (x, card_y, x + card_width, card_y + card_height),
            radius=12,
            fill=(255, 255, 255),
            outline=(210, 216, 225),
            width=1,
        )
        draw.text((x + 18, card_y + 17), title,
                  font=font(21, True), fill=(37, 99, 235))
        screenshot = load(image_name)
        fitted = ImageOps.contain(screenshot, (236, 382), Image.Resampling.LANCZOS)
        background.alpha_composite(
            fitted,
            (x + (card_width - fitted.width) // 2, card_y + 61),
        )

    background.convert("RGB").save(IMAGES / "social-preview.png", optimize=True)


def main() -> None:
    parser = ArgumentParser()
    parser.add_argument("--background", type=Path)
    args = parser.parse_args()
    make_overview()
    make_demo_gif()
    make_social_preview(args.background)


if __name__ == "__main__":
    main()
