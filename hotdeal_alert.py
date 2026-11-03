#!/usr/bin/env python3
"""핫딜 사이트 키워드 알림봇.

config.json 에 적힌 핫딜 게시판들을 읽어서, 규칙에 맞는 새 글이 올라오면
텔레그램 / ntfy / 디스코드로 알림을 보낸다. 이미 알린 글은 seen.json 에 기록해서
중복 알림을 막는다. 파이썬 표준 라이브러리만 사용한다.
"""

from __future__ import annotations

import argparse
import html
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CONFIG_PATH = ROOT / "config.json"
SEEN_PATH = ROOT / "seen.json"

USER_AGENT = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36"
)
SEEN_KEEP_DAYS = 45


# ---------------------------------------------------------------- 가져오기

def fetch(url: str, timeout: int = 20) -> str:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": USER_AGENT,
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language": "ko-KR,ko;q=0.9,en;q=0.8",
        },
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        raw = resp.read()
        charset = resp.headers.get_content_charset()
    return decode(raw, charset)


def decode(raw: bytes, charset: str | None = None) -> str:
    """응답 바이트를 문자열로. 뽐뿌처럼 EUC-KR 인 사이트도 처리한다."""
    candidates = []
    if charset:
        candidates.append(charset)
    head = raw[:2048].decode("ascii", "ignore")
    m = re.search(r"""(?:charset|encoding)\s*=\s*["']?([\w-]+)""", head, re.I)
    if m:
        candidates.append(m.group(1))
    candidates += ["utf-8", "cp949"]
    for enc in candidates:
        enc = enc.lower()
        if enc in ("euc-kr", "euckr", "ks_c_5601-1987"):
            enc = "cp949"  # EUC-KR 의 상위 호환
        try:
            return raw.decode(enc)
        except (LookupError, UnicodeDecodeError):
            continue
    return raw.decode("utf-8", "replace")


# ---------------------------------------------------------------- 파싱

def clean(text: str) -> str:
    text = re.sub(r"<!\[CDATA\[(.*?)\]\]>", r"\1", text, flags=re.S)
    text = re.sub(r"<[^>]+>", " ", text)
    text = html.unescape(text)
    return re.sub(r"\s+", " ", text).strip()


def parse_rss(text: str, base_url: str) -> list[dict]:
    posts = []
    for item in re.findall(r"<item\b.*?</item>", text, flags=re.S | re.I):
        title = re.search(r"<title\b[^>]*>(.*?)</title>", item, re.S | re.I)
        link = re.search(r"<link\b[^>]*>(.*?)</link>", item, re.S | re.I)
        if not title or not link:
            continue
        url = urllib.parse.urljoin(base_url, clean(link.group(1)))
        posts.append({"title": clean(title.group(1)), "url": url})
    return posts


class _AnchorCollector(HTMLParser):
    """페이지의 모든 <a> 태그에서 (href, 글자, title 속성) 을 모은다."""

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.anchors: list[dict] = []
        self._stack: list[dict] = []

    def handle_starttag(self, tag, attrs):
        if tag != "a":
            return
        a = dict(attrs)
        self._stack.append({"href": a.get("href") or "", "attr_title": a.get("title") or "", "text": []})

    def handle_endtag(self, tag):
        if tag == "a" and self._stack:
            self.anchors.append(self._stack.pop())

    def handle_data(self, data):
        for a in self._stack:
            a["text"].append(data)


def parse_html(text: str, base_url: str, link_pattern: str) -> list[dict]:
    """게시글 링크 모양(link_pattern 정규식)에 맞는 <a> 태그를 글 목록으로 만든다.

    사이트 레이아웃이 바뀌어도 글 주소 모양만 같으면 계속 동작한다.
    한 글에 링크가 여러 개(썸네일, 제목, 댓글수)면 가장 긴 글자를 제목으로 쓴다.
    """
    parser = _AnchorCollector()
    parser.feed(text)
    pattern = re.compile(link_pattern)
    best: dict[str, str] = {}
    order: list[str] = []
    for a in parser.anchors:
        href = html.unescape(a["href"])
        if not pattern.search(href):
            continue
        url = canonical_url(urllib.parse.urljoin(base_url, href))
        title = clean(" ".join(a["text"]))
        if len(a["attr_title"]) > len(title):
            title = clean(a["attr_title"])
        if url not in best:
            order.append(url)
            best[url] = title
        elif len(title) > len(best[url]):
            best[url] = title
    return [{"title": best[u], "url": u} for u in order if best[u]]


# 글 주소에 붙는 페이지/검색 파라미터는 같은 글로 취급하려고 지운다.
_DROP_PARAMS = {"page", "divpage", "category", "sca", "sfl", "stx", "od", "po", "s", "type", "pn"}


def canonical_url(url: str) -> str:
    parts = urllib.parse.urlsplit(url)
    query = [
        (k, v)
        for k, v in urllib.parse.parse_qsl(parts.query, keep_blank_values=True)
        if k.lower() not in _DROP_PARAMS
    ]
    return urllib.parse.urlunsplit(
        (parts.scheme, parts.netloc, parts.path, urllib.parse.urlencode(query), "")
    )


def get_posts(source: dict) -> list[dict]:
    text = fetch(source["url"])
    if source["type"] == "rss":
        posts = parse_rss(text, source["url"])
    else:
        posts = parse_html(text, source["url"], source["link_pattern"])
    for p in posts:
        p["url"] = canonical_url(p["url"])
        p["source"] = source["name"]
    return posts


# ---------------------------------------------------------------- 규칙

def normalize(s: str) -> str:
    return re.sub(r"\s+", "", s).lower()


def matches(title: str, rules: list[dict]) -> bool:
    """규칙 중 하나라도 만족하면 True.

    규칙 = include_any(하나 이상 포함) + require_any(있으면 하나 이상 포함)
          + exclude_any(하나라도 있으면 제외). 띄어쓰기·대소문자 무시.
    """
    t = normalize(title)

    def has_any(words):
        return any(normalize(w) in t for w in words)

    for rule in rules:
        if not has_any(rule.get("include_any", [])):
            continue
        if rule.get("require_any") and not has_any(rule["require_any"]):
            continue
        if rule.get("exclude_any") and has_any(rule["exclude_any"]):
            continue
        return True
    return False


# ---------------------------------------------------------------- 알림

def _post(url: str, data: bytes, headers: dict) -> None:
    req = urllib.request.Request(url, data=data, headers={"User-Agent": USER_AGENT, **headers})
    with urllib.request.urlopen(req, timeout=20) as resp:
        resp.read()


def notifiers() -> list[tuple[str, callable]]:
    out = []
    token, chat = os.environ.get("TELEGRAM_BOT_TOKEN"), os.environ.get("TELEGRAM_CHAT_ID")
    if token and chat:
        def telegram(title, body, url):
            payload = {
                "chat_id": chat,
                "text": f"{title}\n{body}\n{url}",
                "disable_web_page_preview": False,
            }
            _post(
                f"https://api.telegram.org/bot{token}/sendMessage",
                json.dumps(payload).encode(),
                {"Content-Type": "application/json"},
            )
        out.append(("telegram", telegram))

    topic = os.environ.get("NTFY_TOPIC")
    if topic:
        server = os.environ.get("NTFY_SERVER", "https://ntfy.sh").rstrip("/")

        def ntfy(title, body, url):
            payload = {"topic": topic, "title": title, "message": body, "click": url, "tags": ["bell"]}
            if url:
                payload["actions"] = [{"action": "view", "label": "글 열기", "url": url}]
            _post(server, json.dumps(payload).encode(), {"Content-Type": "application/json"})
        out.append(("ntfy", ntfy))

    hook = os.environ.get("DISCORD_WEBHOOK_URL")
    if hook:
        def discord(title, body, url):
            _post(
                hook,
                json.dumps({"content": f"**{title}**\n{body}\n{url}"}).encode(),
                {"Content-Type": "application/json"},
            )
        out.append(("discord", discord))
    return out


def send(title: str, body: str, url: str) -> bool:
    """설정된 모든 채널로 보낸다. 하나라도 성공하면 True."""
    ok = False
    for name, fn in notifiers():
        try:
            fn(title, body, url)
            ok = True
        except Exception as e:  # noqa: BLE001
            print(f"  ! {name} 전송 실패: {e}", file=sys.stderr)
    return ok


# ---------------------------------------------------------------- 상태

def load_seen() -> dict[str, float]:
    try:
        return json.loads(SEEN_PATH.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        return {}


def save_seen(seen: dict[str, float]) -> None:
    cutoff = time.time() - SEEN_KEEP_DAYS * 86400
    seen = {u: t for u, t in seen.items() if t >= cutoff}
    SEEN_PATH.write_text(
        json.dumps(seen, ensure_ascii=False, indent=1, sort_keys=True) + "\n", encoding="utf-8"
    )


# ---------------------------------------------------------------- 실행

def run(config: dict) -> int:
    seen = load_seen()
    channels = notifiers()
    if not channels:
        print("알림 채널이 설정되지 않았습니다 (README 참고). 찾은 글만 출력합니다.")

    found = 0
    for source in config["sources"]:
        if not source.get("enabled", True):
            continue
        try:
            posts = get_posts(source)
        except Exception as e:  # noqa: BLE001  사이트 하나가 막혀도 나머지는 계속
            print(f"[{source['name']}] 가져오기 실패: {e}")
            continue
        hits = [p for p in posts if matches(p["title"], config["rules"]) and p["url"] not in seen]
        print(f"[{source['name']}] 글 {len(posts)}개 확인, 새로 찾은 글 {len(hits)}개")
        for p in hits:
            found += 1
            print(f"  → {p['title']}\n    {p['url']}")
            if channels and send(f"🔔 [{p['source']}] 핫딜 알림", p["title"], p["url"]):
                seen[p["url"]] = time.time()

    save_seen(seen)
    print(f"완료: 새 글 {found}개")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--test", action="store_true", help="알림 채널 연결 테스트 메시지만 보낸다")
    args = ap.parse_args()

    if args.test:
        if not notifiers():
            print("알림 채널이 설정되지 않았습니다. GitHub Secrets 를 확인하세요.")
            return 1
        ok = send("✅ 핫딜 알림봇 연결 테스트", "이 메시지가 보이면 설정 완료입니다.", "https://github.com")
        print("테스트 전송 성공" if ok else "테스트 전송 실패")
        return 0 if ok else 1

    config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    return run(config)


if __name__ == "__main__":
    sys.exit(main())
