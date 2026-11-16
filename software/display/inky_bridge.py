#!/usr/bin/env python3

# Display bridge using Pimoroni's Inky Python library.
# See https://github.com/pimoroni/inky
# Spectra 6 examples:
# https://github.com/pimoroni/inky/tree/main/examples/spectra6

import sys
from pathlib import Path

from PIL import Image
from inky.auto import auto


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: inky_bridge.py FRAME.png", file=sys.stderr)
        return 2
    frame_path = Path(sys.argv[1])
    with Image.open(frame_path) as image:
        frame = image.convert("RGB")
    if frame.size != (600, 400):
        print(f"Expected 600x400 image, got {frame.size}", file=sys.stderr)
        return 1
    inky = auto(ask_user=True, verbose=False)
    if tuple(inky.resolution) != (600, 400):
        print(f"Wrong Inky resolution: {inky.resolution}", file=sys.stderr)
        return 1
    if hasattr(inky, "set_border") and hasattr(inky, "BLACK"):
        inky.set_border(inky.BLACK)
    inky.set_image(frame.rotate(180, expand=False))
    inky.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())
