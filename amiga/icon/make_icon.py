#!/usr/bin/env python3
"""Draw OpenSupaplex's GlowIcon (46x46, original art: a red ball on a green
circuit board tile) and write opensupaplex.info and opensupaplex.png.

    python3 amiga/icon/make_icon.py AMICRADLE_CHECKOUT

AMICRADLE_CHECKOUT is an amicradle checkout; its scripts/amiga_icon.py
writes the OS 3.5 icon. The AmiCradle Team. GPL 3, like OpenSupaplex.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

S = 8
W = H = 46
OUT = os.path.dirname(os.path.abspath(__file__))
CLEAR = (0, 0, 0, 0)


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


def vgrad(size, top, bottom):
    g = Image.new("RGBA", size)
    d = ImageDraw.Draw(g)
    for y in range(size[1]):
        d.line([(0, y), (size[0], y)], fill=lerp(top, bottom, y / max(1, size[1] - 1)))
    return g


def shape(size, fn):
    m = Image.new("L", size, 0)
    fn(ImageDraw.Draw(m))
    return m


def paint(base, mask, top, bottom):
    base.alpha_composite(Image.composite(vgrad(base.size, top, bottom),
                                         Image.new("RGBA", base.size, CLEAR), mask))


def fill(base, mask, colour):
    base.alpha_composite(Image.composite(Image.new("RGBA", base.size, colour),
                                         Image.new("RGBA", base.size, CLEAR), mask))


def draw():
    big = (W * S, H * S)
    img = Image.new("RGBA", big, CLEAR)
    k = lambda *v: [c * S for c in v]

    sh = shape(big, lambda d: d.rounded_rectangle(k(5, 9, 43, 45), radius=4 * S, fill=255))
    fill(img, sh.filter(ImageFilter.GaussianBlur(2 * S)), (0, 0, 0, 110))

    # a green circuit board tile
    board = shape(big, lambda d: d.rounded_rectangle(k(3, 6, 41, 42), radius=4 * S, fill=255))
    fill(img, board.filter(ImageFilter.MaxFilter(2 * S + 1)), (8, 30, 14, 255))
    paint(img, board, (40, 150, 70, 255), (16, 80, 34, 255))
    d = ImageDraw.Draw(img)
    gold = (232, 190, 80, 255)
    for line in ((6, 14, 18, 14, 22, 18), (6, 34, 14, 34, 18, 30), (38, 12, 30, 12, 26, 16),
                 (38, 38, 32, 38, 28, 34), (12, 6, 12, 10), (32, 42, 32, 38)):
        d.line(k(*line), fill=gold, width=S + S // 2, joint="curve")
    for x, y in ((6, 14), (6, 34), (38, 12), (38, 38)):
        d.ellipse(k(x - 1.5, y - 1.5, x + 1.5, y + 1.5), fill=gold)

    # a shiny red ball on it
    ball = shape(big, lambda d: d.ellipse(k(12, 13, 34, 35), fill=255))
    fill(img, ball.filter(ImageFilter.MaxFilter(2 * S + 1)), (50, 6, 6, 255))
    g = Image.new("RGBA", big, CLEAR)
    gd = ImageDraw.Draw(g)
    for r in range(11 * S, 0, -S // 2):
        t = r / (11 * S)
        gd.ellipse([(20 * S - r, 21 * S - r), (20 * S + r, 21 * S + r)],
                   fill=lerp((255, 150, 120, 255), (150, 14, 14, 255), t))
    img.alpha_composite(Image.composite(g, Image.new("RGBA", big, CLEAR), ball))
    hi = shape(big, lambda d: d.ellipse(k(16, 16, 22, 21), fill=255))
    fill(img, hi.filter(ImageFilter.GaussianBlur(S)), (255, 255, 255, 200))

    small = img.resize((W, H), Image.LANCZOS)
    px = small.load()
    for y in range(H):
        for x in range(W):
            r, g_, b, a = px[x, y]
            px[x, y] = (r, g_, b, 255) if a >= 110 else CLEAR
    return small


def main():
    sys.path.insert(0, os.path.join(sys.argv[1], "scripts"))
    import amiga_icon as ai
    pic = draw()
    pic.save(os.path.join(OUT, "opensupaplex.png"))
    rgba = pic.tobytes()
    sel = ai.glow_selected(W, H, rgba)
    meta = {"type": 3, "stack": 65536}
    with open(os.path.join(OUT, "opensupaplex.info"), "wb") as f:
        f.write(ai.write_os35(meta, [(W, H, rgba), (W, H, sel)]))


if __name__ == "__main__":
    main()
