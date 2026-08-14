"""Record the real Peek window into a privacy-safe 1280x720 product demo."""

from argparse import ArgumentParser
from ctypes import Structure, byref, sizeof, windll
from ctypes.wintypes import DWORD, HWND, LONG, RECT
from pathlib import Path
import sys
from time import monotonic, sleep

from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageGrab


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "build" / "video-tools"))
import imageio_ffmpeg  # noqa: E402


DWMWA_EXTENDED_FRAME_BOUNDS = 9


class DwmRect(Structure):
    _fields_ = [("left", LONG), ("top", LONG), ("right", LONG), ("bottom", LONG)]


def capture_window(hwnd: HWND) -> Image.Image:
    visible = DwmRect()
    result = windll.dwmapi.DwmGetWindowAttribute(
        hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, byref(visible), DWORD(sizeof(visible)))
    if result != 0:
        fallback = RECT()
        if not windll.user32.GetWindowRect(hwnd, byref(fallback)):
            raise RuntimeError("Unable to read Peek window bounds")
        visible = DwmRect(fallback.left, fallback.top, fallback.right, fallback.bottom)
    return ImageGrab.grab(
        bbox=(visible.left, visible.top, visible.right, visible.bottom),
        all_screens=True,
    ).convert("RGB")


def make_frame(window: Image.Image, elapsed: float, icon: Image.Image,
               empty_window: Image.Image) -> Image.Image:
    canvas = Image.new("RGB", (1280, 720), (11, 23, 40))
    draw = ImageDraw.Draw(canvas)
    regular = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 24)
    semibold = ImageFont.truetype("C:/Windows/Fonts/seguisb.ttf", 48)
    small = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 18)
    shortcut = ImageFont.truetype("C:/Windows/Fonts/seguisb.ttf", 26)

    canvas.paste(icon, (58, 51), icon)
    draw.text((145, 58), "Peek", font=semibold, fill=(245, 248, 252))
    draw.text((59, 151), "Ghost", font=small, fill=(92, 153, 255))

    if elapsed < 4.3:
        title, detail = "A window flashes.", "It is gone before you can read it."
        step = "01"
    elif elapsed < 8.0:
        title, detail = "Press one hotkey.", "Open the latest Ghost event immediately."
        step = "02"
    elif elapsed < 13.0:
        title, detail = "Peek identifies it.", "Process, lifetime, path, and parent."
        step = "03"
    else:
        title, detail = "One action. One answer.", "Native Win32  ·  Offline  ·  Portable"
        step = "04"

    draw.text((59, 222), step, font=small, fill=(111, 145, 190))
    draw.multiline_text((59, 267), title, font=semibold, fill=(245, 248, 252), spacing=8)
    draw.multiline_text((59, 344), detail, font=regular, fill=(161, 178, 202), spacing=8)
    if 4.3 <= elapsed < 13.0:
        draw.rounded_rectangle((59, 447, 416, 513), radius=9,
                               fill=(24, 43, 69), outline=(61, 91, 130))
        draw.text((237, 480), "Ctrl + Shift + G", font=shortcut,
                  fill=(245, 248, 252), anchor="mm")

    display_window = empty_window if elapsed < 4.3 else window
    target_height = 664
    target_width = round(display_window.width * target_height / display_window.height)
    rendered = display_window.resize((target_width, target_height), Image.Resampling.LANCZOS)
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    shadow_draw = ImageDraw.Draw(shadow)
    x, y = 1280 - target_width - 38, 28
    shadow_draw.rounded_rectangle((x + 8, y + 12, x + target_width + 8, y + target_height + 12),
                                  radius=18, fill=(0, 0, 0, 115))
    shadow = shadow.filter(ImageFilter.GaussianBlur(17))
    canvas = Image.alpha_composite(canvas.convert("RGBA"), shadow).convert("RGB")
    canvas.paste(rendered, (x, y))
    return canvas


def main() -> None:
    parser = ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--title", default="Peek")
    parser.add_argument("--duration", type=float, default=17.0)
    parser.add_argument("--fps", type=int, default=10)
    args = parser.parse_args()

    hwnd = windll.user32.FindWindowW(None, args.title)
    deadline = monotonic() + 30
    while not hwnd and monotonic() < deadline:
        sleep(.1)
        hwnd = windll.user32.FindWindowW(None, args.title)
    if not hwnd:
        raise SystemExit("Peek window not found")

    icon = Image.open(ROOT / "docs/images/peek-icon.png").convert("RGBA")
    icon.thumbnail((70, 70), Image.Resampling.LANCZOS)
    empty_window = Image.open(ROOT / "docs/images/peek-demo-ghost-hd.png").convert("RGB")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    writer = imageio_ffmpeg.write_frames(
        str(args.output), (1280, 720), fps=args.fps, codec="libx264",
        pix_fmt_in="rgb24", pix_fmt_out="yuv420p", quality=5,
        output_params=["-movflags", "+faststart", "-preset", "slow"])
    writer.send(None)
    started = monotonic()
    frame_interval = 1 / args.fps
    next_frame = started
    try:
        while True:
            now = monotonic()
            elapsed = now - started
            if elapsed >= args.duration:
                break
            if now < next_frame:
                sleep(next_frame - now)
            window = capture_window(hwnd)
            writer.send(make_frame(window, elapsed, icon, empty_window).tobytes())
            next_frame += frame_interval
    finally:
        writer.close()
    print(f"Recorded {args.output} ({args.duration:.1f}s, {args.fps} fps)")


if __name__ == "__main__":
    main()
