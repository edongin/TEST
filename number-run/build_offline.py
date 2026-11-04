"""index.html + skins/ + voice/ + 글꼴을 한 파일(숫자달리기.html)로 묶는다.

다운로드해서 폰에서 바로 열 수 있도록 인터넷 없이 동작하는 버전을 만든다.
필요: pip install pillow fonttools brotli
사용: python3 build_offline.py [Jua-Regular.ttf 경로]
      python3 build_offline.py --font-from 숫자달리기.html   (이전 한 파일 버전에 든 글꼴을 그대로 씀)
글꼴을 안 주면 기기 기본 글꼴을 쓴다.
"""
import base64, io, json, os, re, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '숫자달리기.html')


def b64(data):
    return base64.b64encode(data).decode('ascii')


def skins(folder='skins'):
    out = {}
    for f in sorted(os.listdir(os.path.join(HERE, folder))):
        n = int(f.split('.')[0])
        im = Image.open(os.path.join(HERE, folder, f)).convert('RGBA')
        buf = io.BytesIO()
        im.save(buf, 'WEBP', quality=85, method=6)  # 투명 배경 유지, PNG보다 훨씬 작음
        out[n] = 'data:image/webp;base64,' + b64(buf.getvalue())
    return out


def voices():
    d = os.path.join(HERE, 'voice')
    return {f[:-4]: b64(open(os.path.join(d, f), 'rb').read()) for f in sorted(os.listdir(d)) if f.endswith('.mp3')}


def font_face(ttf, html):
    # 게임에 실제로 나오는 글자만 남겨서 용량을 줄임
    from fontTools import subset
    chars = set(html) | set(map(chr, range(32, 127))) | set('−★')
    opts = subset.Options(); opts.flavor = 'woff2'; opts.layout_features = ['*']
    font = subset.load_font(ttf, opts)
    sub = subset.Subsetter(opts); sub.populate(text=''.join(chars)); sub.subset(font)
    buf = io.BytesIO(); subset.save_font(font, buf, opts)
    return ('@font-face { font-family: "Jua"; font-display: block; '
            'src: url(data:font/woff2;base64,%s) format("woff2"); }\n' % b64(buf.getvalue()))


def main():
    html = open(os.path.join(HERE, 'index.html'), encoding='utf-8').read()
    # 인터넷 글꼴 링크 제거 → 내장 글꼴로 대체
    html = re.sub(r'<link rel="stylesheet" href="https://fonts\.googleapis\.com[^>]*>\n?', '', html)
    if len(sys.argv) > 2 and sys.argv[1] == '--font-from':
        old = open(sys.argv[2], encoding='utf-8').read()
        m = re.search(r'@font-face \{ font-family: "Jua";[^}]*\}\n', old)
        assert m, '글꼴을 찾지 못함'
        html = html.replace('<style>\n', '<style>\n' + m.group(0), 1)
    elif len(sys.argv) > 1:
        html = html.replace('<style>\n', '<style>\n' + font_face(sys.argv[1], html), 1)
    embed = json.dumps({'skins': skins(), 'back': skins('skins_back'), 'voice': voices()}, separators=(',', ':'))
    html = html.replace("<script>\n'use strict';", '<script>window.EMBED=' + embed + ';</script>\n<script>\n\'use strict\';', 1)
    assert 'window.EMBED=' in html
    open(OUT, 'w', encoding='utf-8').write(html)
    print(OUT, round(os.path.getsize(OUT) / 1e6, 2), 'MB')


if __name__ == '__main__':
    main()
