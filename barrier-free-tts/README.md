# 글자로 말하기 (배리어프리 음성 안내)

글자를 입력하고 **읽어주기**를 누르면 자연스러운 음성으로 읽어주는 웹 프로그램입니다.
고객 → 직원, 직원 → 고객 양방향으로 사용할 수 있습니다.

## 기능

- 입력한 문장을 Google Cloud 음성(Chirp 3 HD)으로 읽어주기
- 여성 / 남성 목소리 선택
- 읽기 속도 조절 (느리게 · 보통 · 빠르게)
- 고객 / 직원 전환, 역할별 **자주 쓰는 문구** 버튼 (편집 가능, 해당 PC 브라우저에 저장)
- 대화 기록과 다시 듣기 — 기록은 화면에만 있고 저장되지 않으며 `상담 종료`를 누르면 지워집니다
- 번역 글자 표시 (한국어 → 영어, 그 외 언어 → 한국어). 번역문은 글자로만 표시됩니다
- 영어로 입력하면 영어 목소리로 읽어줍니다

## 준비물

1. Node.js 18 이상 (추가 패키지 설치 불필요)
2. Google Cloud API 키
   - Google Cloud 콘솔에서 프로젝트를 만들고 **Cloud Text-to-Speech API** 와 **Cloud Translation API** 를 사용 설정
   - `API 및 서비스 > 사용자 인증 정보`에서 API 키 생성 (위 두 API 로만 제한 권장)

## 실행

```bash
cd barrier-free-tts
GOOGLE_API_KEY=발급받은키 node server.js
```

Windows(명령 프롬프트):

```bat
set GOOGLE_API_KEY=발급받은키
node server.js
```

브라우저에서 `http://localhost:3000` 을 엽니다. 같은 네트워크의 태블릿·다른 PC 에서는
`http://서버PC의IP:3000` 으로 접속합니다.

## 설정 (환경변수)

| 이름 | 기본값 | 설명 |
| --- | --- | --- |
| `GOOGLE_API_KEY` | (필수) | Google Cloud API 키 |
| `PORT` | `3000` | 접속 포트 |
| `VOICE_KO_FEMALE` | `ko-KR-Chirp3-HD-Kore,ko-KR-Neural2-A` | 한국어 여성 목소리 (앞에서부터 시도) |
| `VOICE_KO_MALE` | `ko-KR-Chirp3-HD-Charon,ko-KR-Neural2-C` | 한국어 남성 목소리 |
| `VOICE_EN_FEMALE` | `en-US-Chirp3-HD-Kore,en-US-Neural2-F` | 영어 여성 목소리 |
| `VOICE_EN_MALE` | `en-US-Chirp3-HD-Charon,en-US-Neural2-D` | 영어 남성 목소리 |

목소리를 바꾸고 싶으면 Google Cloud 의 음성 목록에서 이름을 골라 위 변수에 넣으면 됩니다.

## 참고

- 입력한 문장은 음성 합성/번역을 위해 Google Cloud 로 전송됩니다. 주민등록번호 등 민감한 정보는 입력하지 않도록 안내해 주세요.
- 인터넷 연결이 필요합니다.
