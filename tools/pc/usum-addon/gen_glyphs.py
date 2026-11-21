# Writes usum_glyphs.h: the letters L and R for the X menu's L and R keys, as signed distance fields (8 steps a
# texel, 128 on the edge, less inside), so they stay sharp at any window size. Each letter is drawn 4 times larger,
# and each texel keeps its distance to the nearest edge of the letter (between a pixel inside and one outside).
#   python3 gen_glyphs.py [font.ttf] > usum_glyphs.h      (default: FullDex's Inter Bold, frontend/fonts/ttf)
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', '..', 'frontend', 'fonts', 'ttf', 'inter-bold.ttf')
S = 4                 # pixels drawn a texel
W, H = 32, 36         # a letter's tile in texels
SIZE = 26             # the font size in texels (Inter's capitals: 19 texels high)

def field(ch):
    im = Image.new('L', (W * S, H * S), 0)
    d = ImageDraw.Draw(im)
    f = ImageFont.truetype(FONT, SIZE * S)
    bb = d.textbbox((0, 0), ch, font=f)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    d.text(((W * S - tw) / 2 - bb[0], (H * S - th) / 2 - bb[1]), ch, fill=255, font=f)   # (centred by its ink)
    px = im.load()
    inside = [[px[x, y] > 127 for x in range(W * S)] for y in range(H * S)]
    # the letter's edge: the middle of each pair of neighbouring pixels, one inside and one outside
    edge = []
    for y in range(H * S):
        for x in range(W * S):
            if x + 1 < W * S and inside[y][x] != inside[y][x + 1]: edge.append((x + 1.0, y + 0.5))
            if y + 1 < H * S and inside[y][x] != inside[y + 1][x]: edge.append((x + 0.5, y + 1.0))
    rows = []
    for ty in range(H):
        row = []
        for tx in range(W):
            cx, cy = (tx + 0.5) * S, (ty + 0.5) * S          # the texel's middle: a corner of 4 pixels
            n = sum(inside[yy][xx] for yy in (int(cy) - 1, int(cy)) for xx in (int(cx) - 1, int(cx)))
            dist = min(math.hypot(ex - cx, ey - cy) for ex, ey in edge) / S
            sd = -dist if n >= 2 else dist                      # in texels, less than 0 inside
            row.append(max(0, min(255, round(128 + sd * 8))))
        rows.append(row)
    return rows

out = ['/* usum_glyphs.h: made by gen_glyphs.py from %s. The letters L and R as signed distance' % os.path.basename(FONT),
       ' * fields: 8 steps a texel, 128 on the edge, less inside; each centred by its ink in a %dx%d tile. */' % (W, H),
       '#define GLYPH_W %d' % W, '#define GLYPH_H %d' % H]
for ch in 'LR':
    out.append('static const unsigned char GLYPH_%s[GLYPH_H][GLYPH_W] = {' % ch)
    for r in field(ch): out.append('    {' + ','.join('%d' % v for v in r) + '},')
    out.append('};')
sys.stdout.write('\n'.join(out) + '\n')
