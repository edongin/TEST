# Builds skate14/index.html (standalone page) and the artifact body from src/game.html + images.json
import json, sys, os
here = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(here, 'game.html'), encoding='utf-8').read()
imgs = open(os.path.join(here, 'images.json'), encoding='utf-8').read()
body = src.replace('/*IMAGES_JSON*/{}', imgs)
open(os.path.join(here, '..', 'artifact.html'), 'w', encoding='utf-8').write(body)
head, rest = body.split('</style>', 1)
full = ('<!doctype html>\n<html lang="ko">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">\n'
        '<meta name="apple-mobile-web-app-capable" content="yes">\n<meta name="mobile-web-app-capable" content="yes">\n'
        + head + '</style>\n</head>\n<body>' + rest + '\n</body>\n</html>\n')
open(os.path.join(here, '..', 'index.html'), 'w', encoding='utf-8').write(full)
print('ok', len(full))
