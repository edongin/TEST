import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import hotdeal_alert as h  # noqa: E402

CONFIG = json.loads((Path(__file__).resolve().parent.parent / "config.json").read_text(encoding="utf-8"))
RULES = CONFIG["rules"]


class MatchTest(unittest.TestCase):
    def test_hits(self):
        for title in [
            "[알리익스프레스] 레노버 샤오신패드 프로 12.7 2025 (8GB/128GB) ($245.00/무료)",
            "[광군제] Lenovo Xiaoxin Pad Pro 12.7 글로벌롬",
            "레노버 샤오신 패드 프로 12.7 역대가",
            "[알리] 레노버 小新Pad Pro 12.7",
            "레노버 태블릿 12.7 인치 특가",
        ]:
            self.assertTrue(h.matches(title, RULES), title)

    def test_misses(self):
        for title in [
            "[알리] 레노버 샤오신 프로14 노트북 2024",
            "[쿠팡] 갤럭시 탭 S10 FE",
            "삼성 오디세이 모니터",
        ]:
            self.assertFalse(h.matches(title, RULES), title)

    def test_exclude(self):
        rules = [{"include_any": ["샤오신"], "exclude_any": ["품절"]}]
        self.assertFalse(h.matches("샤오신패드 (품절)", rules))
        self.assertTrue(h.matches("샤오신패드", rules))


class ParseTest(unittest.TestCase):
    def test_rss_euc_kr(self):
        xml = (
            '<?xml version="1.0" encoding="euc-kr"?><rss><channel><title>뽐뿌</title>'
            "<item><title><![CDATA[[알리] 샤오신패드 프로 12.7 &amp; 케이스]]></title>"
            "<link>http://www.ppomppu.co.kr/zboard/view.php?id=ppomppu4&amp;no=123</link></item>"
            "</channel></rss>"
        ).encode("cp949")
        posts = h.parse_rss(h.decode(xml), "https://www.ppomppu.co.kr/rss.php?id=ppomppu4")
        self.assertEqual(len(posts), 1)
        self.assertEqual(posts[0]["title"], "[알리] 샤오신패드 프로 12.7 & 케이스")
        self.assertEqual(posts[0]["url"], "http://www.ppomppu.co.kr/zboard/view.php?id=ppomppu4&no=123")

    def test_html_groups_links_per_post(self):
        page = """
        <div class="list">
          <a href="/bbs/qb_saleinfo/views/111?page=2"><img src="t.jpg"></a>
          <a href="/bbs/qb_saleinfo/views/111?page=2" class="subject-link">
             <span class="ellipsis">[알리] 샤오신패드 프로 12.7</span> <span>12</span></a>
          <a href="/bbs/qb_saleinfo/views/222">갤럭시 탭</a>
          <a href="/bbs/notice">공지</a>
        </div>"""
        posts = h.parse_html(page, "https://quasarzone.com/bbs/qb_saleinfo", r"/bbs/qb_saleinfo/views/\d+")
        self.assertEqual(
            posts,
            [
                {"title": "[알리] 샤오신패드 프로 12.7 12", "url": "https://quasarzone.com/bbs/qb_saleinfo/views/111"},
                {"title": "갤럭시 탭", "url": "https://quasarzone.com/bbs/qb_saleinfo/views/222"},
            ],
        )

    def test_html_title_attribute(self):
        page = '<a href="/service/board/jirum/1?od=T31" title="샤오신 패드 핫딜"><span>샤오신…</span></a>'
        posts = h.parse_html(page, "https://www.clien.net/service/board/jirum", r"/service/board/jirum/\d+")
        self.assertEqual(posts[0], {"title": "샤오신 패드 핫딜", "url": "https://www.clien.net/service/board/jirum/1"})

    def test_config_patterns_compile(self):
        import re
        for s in CONFIG["sources"]:
            self.assertIn(s["type"], ("rss", "html"))
            if s["type"] == "html":
                re.compile(s["link_pattern"])


class RunTest(unittest.TestCase):
    def test_marks_seen_only_after_send(self):
        sent = []
        posts = [{"title": "샤오신패드 프로 12.7", "url": "https://x/1", "source": "테스트"}]
        orig = (h.get_posts, h.notifiers, h.load_seen, h.save_seen)
        saved = {}
        try:
            h.get_posts = lambda s: [dict(p) for p in posts]
            h.notifiers = lambda: [("fake", lambda t, b, u: sent.append(u))]
            h.load_seen = lambda: {}
            h.save_seen = lambda seen: saved.update(seen)
            h.run({"rules": RULES, "sources": [{"name": "테스트"}]})
        finally:
            h.get_posts, h.notifiers, h.load_seen, h.save_seen = orig
        self.assertEqual(sent, ["https://x/1"])
        self.assertIn("https://x/1", saved)


if __name__ == "__main__":
    unittest.main()
