"""Capture a visible Windows window at native physical-pixel resolution."""

from argparse import ArgumentParser
from ctypes import Structure, byref, create_string_buffer, sizeof, windll
from ctypes.wintypes import BYTE, DWORD, HWND, LONG, RECT, UINT, WORD
from pathlib import Path
from time import monotonic, sleep

from PIL import Image, ImageGrab


DWMWA_EXTENDED_FRAME_BOUNDS = 9
PROCESS_PER_MONITOR_DPI_AWARE = 2
PW_RENDERFULLCONTENT = 2
SRCCOPY = 0x00CC0020


class DwmRect(Structure):
    _fields_ = [("left", LONG), ("top", LONG), ("right", LONG), ("bottom", LONG)]


class BitmapInfoHeader(Structure):
    _fields_ = [
        ("biSize", DWORD),
        ("biWidth", LONG),
        ("biHeight", LONG),
        ("biPlanes", WORD),
        ("biBitCount", WORD),
        ("biCompression", DWORD),
        ("biSizeImage", DWORD),
        ("biXPelsPerMeter", LONG),
        ("biYPelsPerMeter", LONG),
        ("biClrUsed", DWORD),
        ("biClrImportant", DWORD),
    ]


class RgbQuad(Structure):
    _fields_ = [("rgbBlue", BYTE), ("rgbGreen", BYTE), ("rgbRed", BYTE), ("rgbReserved", BYTE)]


class BitmapInfo(Structure):
    _fields_ = [("bmiHeader", BitmapInfoHeader), ("bmiColors", RgbQuad * 1)]


def print_window(hwnd: HWND, visible: DwmRect) -> Image.Image | None:
    window = RECT()
    if not windll.user32.GetWindowRect(hwnd, byref(window)):
        return None
    width = window.right - window.left
    height = window.bottom - window.top
    source_dc = windll.user32.GetWindowDC(hwnd)
    memory_dc = windll.gdi32.CreateCompatibleDC(source_dc)
    bitmap = windll.gdi32.CreateCompatibleBitmap(source_dc, width, height)
    previous = windll.gdi32.SelectObject(memory_dc, bitmap)
    try:
        if not windll.user32.PrintWindow(hwnd, memory_dc, PW_RENDERFULLCONTENT):
            return None
        info = BitmapInfo()
        info.bmiHeader.biSize = sizeof(BitmapInfoHeader)
        info.bmiHeader.biWidth = width
        info.bmiHeader.biHeight = -height
        info.bmiHeader.biPlanes = 1
        info.bmiHeader.biBitCount = 32
        pixels = create_string_buffer(width * height * 4)
        if not windll.gdi32.GetDIBits(
            memory_dc, bitmap, 0, height, pixels, byref(info), UINT(0)
        ):
            return None
        image = Image.frombuffer("RGB", (width, height), pixels, "raw", "BGRX", 0, 1)
        crop = (
            visible.left - window.left,
            visible.top - window.top,
            visible.right - window.left,
            visible.bottom - window.top,
        )
        return image.crop(crop)
    finally:
        windll.gdi32.SelectObject(memory_dc, previous)
        windll.gdi32.DeleteObject(bitmap)
        windll.gdi32.DeleteDC(memory_dc)
        windll.user32.ReleaseDC(hwnd, source_dc)


def capture_screen_region(bounds: DwmRect) -> Image.Image | None:
    width = bounds.right - bounds.left
    height = bounds.bottom - bounds.top
    source_dc = windll.user32.GetDC(0)
    memory_dc = windll.gdi32.CreateCompatibleDC(source_dc)
    bitmap = windll.gdi32.CreateCompatibleBitmap(source_dc, width, height)
    previous = windll.gdi32.SelectObject(memory_dc, bitmap)
    try:
        if not windll.gdi32.BitBlt(
            memory_dc, 0, 0, width, height, source_dc, bounds.left, bounds.top, SRCCOPY
        ):
            return None
        info = BitmapInfo()
        info.bmiHeader.biSize = sizeof(BitmapInfoHeader)
        info.bmiHeader.biWidth = width
        info.bmiHeader.biHeight = -height
        info.bmiHeader.biPlanes = 1
        info.bmiHeader.biBitCount = 32
        pixels = create_string_buffer(width * height * 4)
        if not windll.gdi32.GetDIBits(
            memory_dc, bitmap, 0, height, pixels, byref(info), UINT(0)
        ):
            return None
        return Image.frombuffer("RGB", (width, height), pixels, "raw", "BGRX", 0, 1)
    finally:
        windll.gdi32.SelectObject(memory_dc, previous)
        windll.gdi32.DeleteObject(bitmap)
        windll.gdi32.DeleteDC(memory_dc)
        windll.user32.ReleaseDC(0, source_dc)


def main() -> None:
    parser = ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--title", default="Peek")
    parser.add_argument("--trigger", type=Path)
    parser.add_argument("--wait-for-window", action="store_true")
    parser.add_argument("--settle-ms", type=int, default=0)
    args = parser.parse_args()

    if args.trigger:
        deadline = monotonic() + 120
        while not args.trigger.exists():
            if monotonic() >= deadline:
                raise SystemExit(f"Timed out waiting for trigger: {args.trigger}")
            sleep(0.05)

    # Request physical coordinates before locating or capturing the window.
    try:
        windll.shcore.SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)
    except OSError:
        pass

    hwnd: HWND = windll.user32.FindWindowW(None, args.title)
    if args.wait_for_window:
        deadline = monotonic() + 120
        while not hwnd:
            if monotonic() >= deadline:
                raise SystemExit(f"Timed out waiting for window: {args.title}")
            sleep(0.05)
            hwnd = windll.user32.FindWindowW(None, args.title)
    if not hwnd:
        raise SystemExit(f"Window not found: {args.title}")
    if args.settle_ms > 0:
        sleep(args.settle_ms / 1000)

    bounds = DwmRect()
    result = windll.dwmapi.DwmGetWindowAttribute(
        hwnd,
        DWMWA_EXTENDED_FRAME_BOUNDS,
        byref(bounds),
        DWORD(sizeof(bounds)),
    )
    if result != 0:
        fallback = RECT()
        if not windll.user32.GetWindowRect(hwnd, byref(fallback)):
            raise SystemExit("Unable to read window bounds")
        bounds = DwmRect(fallback.left, fallback.top, fallback.right, fallback.bottom)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    image = capture_screen_region(bounds)
    if image is None:
        box = (bounds.left, bounds.top, bounds.right, bounds.bottom)
        image = ImageGrab.grab(bbox=box, all_screens=True)
    image.save(args.output, optimize=True)
    print(f"{args.output}: {image.width}x{image.height}")


if __name__ == "__main__":
    main()
