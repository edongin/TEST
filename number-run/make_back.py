"""정면 스킨(skins/N.png)으로 뒷모습 스킨(skins_back/N.png)을 만든다.

- 팔다리를 뺀 몸통(블록 부분)만 남기고, 얼굴은 지우고 같은 색 블록으로 채운다.
- 몸통 위에 떠 있는 것(숫자 표시, 모자, 머리카락)은 그대로 둔다.
- 팔다리는 게임에서 따로 그려서 흔들리게 하므로, 몸통 위치와 팔다리 색을 index.html의 BACK_META에 적는다.
필요: pip install pillow numpy scipy
사용: python3 make_back.py
"""
import json, os, re
import numpy as np
from PIL import Image
from scipy import ndimage as ndi

HERE = os.path.dirname(os.path.abspath(__file__))
# 그림이 기울었거나 입체라서 칸이 안 맞는 숫자: 몸통 모양 그대로 씀
LOOSE = {8, 27, 33, 54, 72}
# 원래 직사각형인데 팔·가방 때문에 모양이 어긋나는 숫자: 가장 흔한 가로 폭으로 직사각형을 채움
RECT = {21, 26, 34, 37, 39, 41, 42, 56, 70}
SRC, DST = os.path.join(HERE, 'skins'), os.path.join(HERE, 'skins_back')


def disk(r):
    r = max(1, int(round(r)))
    y, x = np.ogrid[-r:r + 1, -r:r + 1]
    return x * x + y * y <= r * r


def cell_color(px):
    """칸 안에서 가장 많은 색(색상 기준으로 묶음)의 평균. 얼굴(흰자·눈동자·안경)보다 몸 색이 넓다."""
    if len(px) == 0:
        return np.array([200, 200, 200], np.float32)
    p = px / 255.0
    mx, mn = p.max(1), p.min(1)
    sat = np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-6), 0)
    r, g, b = p[:, 0], p[:, 1], p[:, 2]
    d = np.maximum(mx - mn, 1e-6)
    h = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) / 6
    key = np.where(sat > 0.22, (h * 18).astype(int) % 18, 18 + (mx * 4).astype(int).clip(0, 3))
    vals, cnt = np.unique(key, return_counts=True)
    sel = key == vals[np.argmax(cnt)]
    c = px[sel]
    # 그림자 진 쪽보다 밝은 쪽 색에 가깝게
    lum = c.sum(1)
    return c[lum >= np.median(lum)].mean(0)


def fix_face_cells(colors):
    """큰 숫자는 얼굴이 여러 칸에 걸침. 주변 칸 대부분이 같은 색인데 혼자 튀는 칸은 주변 색으로."""
    if len(colors) < 9:
        return
    near = lambda p, q: np.abs(p - q).sum() < 60
    for _ in range(4):
        changed = False
        for (i, j), c in list(colors.items()):
            nb = [colors[(i + di, j + dj)] for di in (-1, 0, 1) for dj in (-1, 0, 1)
                  if (di or dj) and (i + di, j + dj) in colors]
            if len(nb) < 6:
                continue
            for ref in nb:
                same = [q for q in nb if near(q, ref)]
                if len(same) >= 5 and not near(c, ref):
                    colors[(i, j)] = np.mean(same, 0); changed = True
                    break
        if not changed:
            break
    # 대부분이 한 색(예: 100은 전부 빨강)인데 흰색·검은색 칸이 섞여 있으면 얼굴 → 그 색으로
    vals = list(colors.values())
    lowsat = lambda c: (c.max() - c.min()) < 0.25 * max(c.max(), 1) or c.max() < 70
    for ref in vals:
        same = [q for q in vals if near(q, ref)]
        if len(same) >= 0.6 * len(vals) and not lowsat(ref):
            d = np.mean(same, 0)
            for k, c in colors.items():
                if lowsat(c):
                    colors[k] = d
            break


def body_cells(a, n):
    """팔다리를 깎아낸 몸통을 블록 칸으로 맞춘다. (칸 목록, 칸 크기, 격자 원점)"""
    u = np.sqrt(a.sum() / (1.4 * n))
    for _ in range(3):
        op = ndi.binary_opening(a, structure=disk(max(u * 0.42, a.shape[0] * 0.034)))
        lab, k = ndi.label(op)
        if k == 0:
            break
        sizes = ndi.sum(op, lab, range(1, k + 1))
        body = lab == (np.argmax(sizes) + 1)
        u = np.sqrt(body.sum() / n)
    ys, xs = np.nonzero(body)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    cols = max(1, round((x1 - x0) / u)); rows = max(1, round((y1 - y0) / u))
    cw, ch = (x1 - x0) / cols, (y1 - y0) / rows
    cand = []
    for j in range(rows):
        for i in range(cols):
            ya, yb, xa, xb = int(y0 + j * ch), int(y0 + (j + 1) * ch), int(x0 + i * cw), int(x0 + (i + 1) * cw)
            cand.append((body[ya:yb, xa:xb].mean(), (i, j, xa, ya, xb, yb)))
    # 블록 수는 숫자와 같으니, 몸통이 가장 많이 덮은 칸 n개를 고름 (기울어진 그림 대비)
    cand.sort(key=lambda t: -t[0])
    cells = [c for cov, c in cand if cov > 0.5]
    if len(cells) > n:
        cells = [c for cov, c in cand[:n]]
    cells = trim_bumps(cells)
    if n in RECT:
        cells = to_rect(cells, x0, y0, cw, ch)
    return cells, cw, ch, (x0, y0, x1, y1), body


def to_rect(cells, x0, y0, cw, ch):
    from collections import Counter
    have = {(c[0], c[1]) for c in cells}
    spans = Counter()
    for j in {j for _, j in have}:
        iis = [i for i, jj in have if jj == j]
        spans[(min(iis), max(iis))] += 1
    (ia, ib), _ = spans.most_common(1)[0]
    jt, jb = min(j for _, j in have), max(j for _, j in have)
    return [(i, j, int(x0 + i * cw), int(y0 + j * ch), int(x0 + (i + 1) * cw), int(y0 + (j + 1) * ch))
            for j in range(jt, jb + 1) for i in range(ia, ib + 1)]


def trim_bumps(cells):
    """옆으로 튀어나온 칸(팔 자국)을 지운다: 맨 위·맨 아래 줄에 닿지 않는 바깥쪽 짧은 세로 줄."""
    for _ in range(4):
        have = {(i, j) for (i, j, *_r) in cells}
        if not have:
            return cells
        jt, jb = min(j for _, j in have), max(j for _, j in have)
        rows = jb - jt + 1
        drop = set()
        cols = sorted({i for i, _ in have})
        for i in cols:
            js = sorted(j for ii, j in have if ii == i)
            runs, cur = [], [js[0]]
            for j in js[1:]:
                if j == cur[-1] + 1: cur.append(j)
                else: runs.append(cur); cur = [j]
            runs.append(cur)
            for r in runs:
                if r[0] == jt or r[-1] == jb or len(r) > max(1, rows * 0.4):
                    continue
                outer = all((i - 1, j) not in have for j in r) or all((i + 1, j) not in have for j in r)
                if outer:
                    drop.update((i, j) for j in r)
        if not drop:
            break
        cells = [c for c in cells if (c[0], c[1]) not in drop]
    return cells





def make(n):
    im = Image.open(os.path.join(SRC, '%d.png' % n)).convert('RGBA')
    arr = np.asarray(im).astype(np.float32)
    a = arr[..., 3] > 128
    H, W = a.shape
    cells, cw, ch, (x0, y0, x1, y1), core = body_cells(a, n)
    rgb = arr[..., :3]
    out = np.zeros_like(arr)
    if n in LOOSE:
        return make_loose(n, arr, a, core, cw, ch)
    body = np.zeros((H, W), bool)
    xx = (np.arange(W) - (x0 + x1) / 2) / max(1, (x1 - x0) / 2)
    shade = 1 - 0.12 * np.clip(np.abs(xx), 0, 1) ** 2
    colors = {}
    for (i, j, xa, ya, xb, yb) in cells:
        m = core[ya:yb, xa:xb]  # 팔다리를 깎아낸 몸통 픽셀만으로 색을 고름
        if m.sum() < 10:
            m = a[ya:yb, xa:xb]
        colors[(i, j)] = cell_color(rgb[ya:yb, xa:xb][m])
    fix_face_cells(colors)
    for (i, j, xa, ya, xb, yb) in cells:
        c = colors[(i, j)]
        blk = np.ones((yb - ya, xb - xa, 3), np.float32) * c
        blk *= shade[None, xa:xb, None]
        # 블록 이음선
        e = max(1, int(min(cw, ch) * 0.03))
        blk[:e] *= 0.88; blk[-e:] *= 0.82; blk[:, :e] *= 0.9; blk[:, -e:] *= 0.84
        out[ya:yb, xa:xb, :3] = blk; out[ya:yb, xa:xb, 3] = 255
        body[ya:yb, xa:xb] = True
    # 몸통 바깥 테두리
    rim = body & ~ndi.binary_erosion(body, structure=disk(max(1, min(cw, ch) * 0.035)))
    out[rim, :3] *= 0.72

    # 몸통 위쪽에 떠 있는 것(숫자 표시·모자·머리카락)은 원래 그림 그대로
    above = a & ~body
    above[int(y0 + ch * 0.1):, :] = False
    out[above] = arr[above]

    Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGBA').save(os.path.join(DST, '%d.png' % n), optimize=True)
    js = [j for (_, j) in colors]
    top = [c for (i, j), c in colors.items() if j == min(js)]
    bot = [c for (i, j), c in colors.items() if j == max(js)]
    hexc = lambda c, k: '#%02x%02x%02x' % tuple(int(v) for v in np.clip(np.mean(c, 0) * k, 0, 255))
    return {
        'b': [round(x0 / W, 4), round(y0 / H, 4), round(x1 / W, 4), round(y1 / H, 4)],
        'u': round(min(cw, ch) / H, 4),
        'arm': hexc(top, 0.8), 'leg': hexc(bot, 0.7),
    }


def make_loose(n, arr, a, core, cw, ch):
    """그림이 기울었거나 가방 등이 있어서 칸이 잘 안 맞을 때: 몸통 모양 그대로, 가로 띠마다 색을 채움."""
    H, W = a.shape
    rgb = arr[..., :3]
    ys, xs = np.nonzero(core)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    u = min(cw, ch)
    out = np.zeros_like(arr)
    xx = (np.arange(W) - (x0 + x1) / 2) / max(1, (x1 - x0) / 2)
    shade = 1 - 0.12 * np.clip(np.abs(xx), 0, 1) ** 2
    cols = []
    y = y0
    while y < y1:
        yb = min(y1, int(y + u))
        m = core[y:yb]
        c = cell_color(rgb[y:yb][m]) if m.sum() > 10 else (cols[-1] if cols else np.array([200, 200, 200.]))
        cols.append(c)
        band = np.ones((yb - y, W, 3), np.float32) * c * shade[None, :, None]
        band[:max(1, int(u * 0.03))] *= 0.88
        out[y:yb, :, :3] = band
        y = yb
    out[..., 3] = 0
    out[core, 3] = 255
    out[~core, :3] = 0
    rim = core & ~ndi.binary_erosion(core, structure=disk(max(1, u * 0.035)))
    out[rim, :3] *= 0.72
    above = a & ~core
    above[int(y0 + ch * 0.1):, :] = False
    out[above] = arr[above]
    Image.fromarray(out.clip(0, 255).astype(np.uint8), 'RGBA').save(os.path.join(DST, '%d.png' % n), optimize=True)
    hexc = lambda c, k: '#%02x%02x%02x' % tuple(int(v) for v in np.clip(c * k, 0, 255))
    return {
        'b': [round(x0 / W, 4), round(y0 / H, 4), round(x1 / W, 4), round(y1 / H, 4)],
        'u': round(u / H, 4), 'arm': hexc(cols[min(1, len(cols) - 1)], 0.8), 'leg': hexc(cols[-1], 0.7), 'loose': 1,
    }


def main():
    os.makedirs(DST, exist_ok=True)
    meta = {}
    for f in sorted(os.listdir(SRC), key=lambda s: int(s.split('.')[0])):
        n = int(f.split('.')[0])
        meta[n] = make(n)
    # index.html 안의 BACK_META 줄을 새 값으로 바꿈
    path = os.path.join(HERE, 'index.html')
    html = open(path, encoding='utf-8').read()
    line = 'const BACK_META = ' + json.dumps({str(k): v for k, v in meta.items()}, separators=(',', ':')) + ';'
    html, k = re.subn(r'^const BACK_META = .*;$', lambda m: line, html, count=1, flags=re.M)
    assert k == 1, 'index.html에 const BACK_META 줄이 없음'
    open(path, 'w', encoding='utf-8').write(html)
    print(len(meta), 'back skins')


if __name__ == '__main__':
    main()
