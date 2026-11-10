# 핫딜 알림봇 (샤오신패드 프로 12.7)

국내 핫딜 게시판을 **10분마다** 확인해서, 제목에 샤오신패드 관련 글이 올라오면 휴대폰으로 알림을 보냅니다.
GitHub Actions에서 돌아가므로 컴퓨터를 켜 둘 필요가 없고, 공개 레포라 무료입니다.

**확인하는 게시판:** 뽐뿌(국내·해외·알리), 루리웹 핫딜, 퀘이사존, 클리앙 알뜰구매, 딜바다 해외, 에펨코리아 핫딜, 아카라이브 핫딜

**알림 조건 (`config.json`):**
- `샤오신 / 小新 / xiaoxin` + `패드 / pad / 탭 / 태블릿 / 12.7` 이 제목에 함께 있을 때 (샤오신 노트북 글은 무시)
- 또는 `레노버 / lenovo` + `12.7` 이 함께 있을 때

---

## 설정 방법 (5분)

알림 받을 방법을 **하나만** 고르면 됩니다. 가장 쉬운 건 ntfy입니다.

### 방법 A. ntfy 앱 (가입 없음, 가장 간단)
1. 휴대폰에 **ntfy** 앱 설치 (Play 스토어 / App Store)
2. 앱에서 `+` → 남들이 못 맞힐 주제 이름 입력 (예: `xiaoxin-alert-8f3k2q`) → 구독
3. GitHub 레포 → **Settings → Secrets and variables → Actions → New repository secret**
   - Name: `NTFY_TOPIC` / Secret: 위에서 정한 주제 이름

### 방법 B. 텔레그램
1. 텔레그램에서 `@BotFather` 검색 → `/newbot` → 봇 이름 정하기 → **토큰** 복사
2. 방금 만든 봇에게 아무 메시지나 하나 보내기
3. 브라우저로 `https://api.telegram.org/bot<토큰>/getUpdates` 열기 → `"chat":{"id":123456789` 의 숫자 복사
4. GitHub Secrets 에 `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID` 두 개 등록

### 방법 C. 디스코드
채널 설정 → 연동 → 웹후크 만들기 → URL 복사 → Secret `DISCORD_WEBHOOK_URL` 로 등록

### 연결 테스트
레포 → **Actions → 핫딜 알림 → Run workflow** → `알림 연결 테스트 메시지만 보내기` 체크 → 실행.
"✅ 핫딜 알림봇 연결 테스트" 알림이 오면 끝입니다.

> ⚠️ 10분마다 자동 실행은 **기본 브랜치(main)** 에 있을 때만 동작합니다. 이 브랜치를 main 에 합쳐 주세요.

---

## 알아두면 좋은 점
- 같은 글은 한 번만 알립니다. 알린 글 목록은 `seen.json` 에 저장되고 45일 뒤 지워집니다.
- GitHub 사정으로 실행이 몇 분 늦어질 수 있습니다 (특히 정각 무렵).
- 에펨코리아·아카라이브는 해외 서버 접속을 막는 경우가 있어 실패할 수 있습니다. 실패한 사이트만 건너뛰고 나머지는 계속 확인합니다. Actions 실행 기록에서 사이트별 결과를 볼 수 있습니다.
- 공개 레포에 60일 동안 활동이 없으면 GitHub이 자동 실행을 멈춥니다. 광군제 이후 필요 없으면 Actions 에서 워크플로를 **Disable** 하면 됩니다.

## 키워드 바꾸기
`config.json` 의 `rules` 를 고치면 됩니다. 띄어쓰기·대소문자는 무시합니다.

```json
{
  "include_any": ["샤오신"],          // 이 중 하나는 꼭 있어야 함
  "require_any": ["패드", "12.7"],    // (선택) 이 중 하나도 있어야 함
  "exclude_any": ["품절", "종료"]     // (선택) 이 중 하나라도 있으면 제외
}
```

## 로컬에서 실행
```bash
NTFY_TOPIC=내주제 python3 hotdeal_alert.py
```

```bash
python3 -m unittest discover -s tests
```

---

## 다른 프로젝트

- [버스 도착 알림판 (Pico W / ESP32)](esp32-bus-display/README.md): 공공데이터포털 버스 도착정보 API로 집 앞 정류장의 버스 도착 시간을 ILI9341 화면에 보여줍니다.
