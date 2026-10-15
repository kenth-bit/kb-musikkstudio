#!/usr/bin/env python3
"""Lager resources/icon.ico (ikonet til KB Musikkstudio) uten eksterne pakker.

Tegner et mørkt avrundet felt med en neon sagtannbølge, som er lyden synthen lager.
Kjør:  python3 tools/make_icon.py
Endre fargene under hvis du vil ha et annet uttrykk."""
import math
import pathlib
import struct

BACKGROUND = (12, 11, 18)
PANEL = (29, 27, 42)
BORDER = (155, 92, 255)
CYAN = (63, 224, 255)
MAGENTA = (255, 79, 216)
SIZES = (16, 24, 32, 48, 64, 128, 256)
SUPERSAMPLE = 3


def rounded_box_distance(x, y, half, radius):
    qx = abs(x) - (half - radius)
    qy = abs(y) - (half - radius)
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    inside = min(max(qx, qy), 0.0)
    return outside + inside - radius


def saw(phase):
    return 2.0 * (phase - math.floor(phase + 0.5))


def distance_to_segment(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    length_sq = dx * dx + dy * dy
    t = 0.0 if length_sq == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_sq))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def wave_points(count_teeth=2.0, steps=64):
    points = []
    for i in range(steps * int(count_teeth) + 1):
        u = i / (steps * count_teeth)
        phase = u * count_teeth
        points.append((-0.62 + 1.24 * u, -0.30 * saw(phase - 0.5)))
    return points


def mix(a, b, t):
    return tuple(a[i] * (1 - t) + b[i] * t for i in range(3))


def render(size):
    big = size * SUPERSAMPLE
    half = 1.0
    radius = 0.42
    points = wave_points()
    segments = list(zip(points[:-1], points[1:]))
    pixels = []

    for py in range(big):
        row = []
        for px in range(big):
            x = (px + 0.5) / big * 2.0 - 1.0
            y = (py + 0.5) / big * 2.0 - 1.0
            d = rounded_box_distance(x, y, half, radius)

            if d > 0.0:
                row.append((0, 0, 0, 0.0))
                continue

            glow_t = 1.0 - min(1.0, (y + 1.0) / 2.0)
            colour = mix(BACKGROUND, PANEL, 0.35 + 0.65 * glow_t * 0.5)

            if d > -0.07:
                colour = mix(colour, BORDER, min(1.0, (d + 0.07) / 0.07) * 0.9)

            nearest = min(distance_to_segment(x, y, a[0], a[1], b[0], b[1]) for a, b in segments)
            line_colour = mix(CYAN, MAGENTA, (x + 0.62) / 1.24 if -0.62 <= x <= 0.62 else 0.0)
            glow = math.exp(-(nearest / 0.11) ** 2) * 0.55
            colour = mix(colour, line_colour, min(1.0, glow))
            core = max(0.0, 1.0 - nearest / 0.045)
            colour = mix(colour, (255, 255, 255), core * 0.35)
            colour = mix(colour, line_colour, core * 0.65)
            row.append((colour[0], colour[1], colour[2], 1.0))
        pixels.append(row)

    out = []
    for oy in range(size):
        line = []
        for ox in range(size):
            r = g = b = a = 0.0
            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    pr, pg, pb, pa = pixels[oy * SUPERSAMPLE + sy][ox * SUPERSAMPLE + sx]
                    r += pr * pa
                    g += pg * pa
                    b += pb * pa
                    a += pa
            if a == 0:
                line.append((0, 0, 0, 0))
            else:
                n = SUPERSAMPLE * SUPERSAMPLE
                line.append((round(r / a), round(g / a), round(b / a), round(255 * a / n)))
        out.append(line)
    return out


def bmp_dib(rows):
    """Klassisk BMP-ikon. JUCE på Windows leser ikke ikoner som er lagret som PNG."""
    size = len(rows)
    xor_rows = []
    for row in reversed(rows):
        xor_rows.append(b''.join(struct.pack('<BBBB', pixel[2], pixel[1], pixel[0], pixel[3]) for pixel in row))
    mask_stride = ((size + 31) // 32) * 4
    mask = b'\x00' * (mask_stride * size)
    image = b''.join(xor_rows) + mask
    header = struct.pack('<IIIHHIIIIII', 40, size, size * 2, 1, 32, 0, len(image), 0, 0, 0, 0)
    return header + image


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    target = root / 'resources' / 'icon.ico'
    target.parent.mkdir(parents=True, exist_ok=True)

    images = [(size, bmp_dib(render(size))) for size in SIZES]
    header = struct.pack('<HHH', 0, 1, len(images))
    offset = 6 + 16 * len(images)
    directory = b''
    data = b''

    for size, dib in images:
        stored = 0 if size >= 256 else size
        directory += struct.pack('<BBBBHHII', stored, stored, 0, 0, 1, 32, len(dib), offset + len(data))
        data += dib

    target.write_bytes(header + directory + data)
    print(f'Skrev {target} ({target.stat().st_size} byte, {len(images)} størrelser)')


if __name__ == '__main__':
    main()
