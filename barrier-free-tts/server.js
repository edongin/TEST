// 배리어프리 글자-음성 안내 서버
// 외부 패키지 없이 Node.js 18+ 만으로 동작합니다.
// 역할: 화면(public/) 제공 + Google Cloud 음성합성/번역 호출 대행(API 키를 브라우저에 노출하지 않기 위함)

const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');

const PORT = Number(process.env.PORT || 3000);
const HOST = process.env.HOST || '0.0.0.0';
const API_KEY = process.env.GOOGLE_API_KEY || '';
const TTS_URL = process.env.GOOGLE_TTS_URL || 'https://texttospeech.googleapis.com/v1/text:synthesize';
const TRANSLATE_URL = process.env.GOOGLE_TRANSLATE_URL || 'https://translation.googleapis.com/language/translate/v2';
const MAX_TEXT = 1000;

// 목소리 목록: 앞에서부터 시도하고, 실패하면 다음 목소리로 넘어갑니다.
// Chirp 3 HD 가 가장 자연스럽고, Neural2 는 예비용입니다.
// 환경변수(VOICE_KO_FEMALE 등)로 바꿀 수 있습니다. 쉼표로 여러 개 지정 가능.
const list = (env, fallback) => (process.env[env] || fallback).split(',').map((s) => s.trim()).filter(Boolean);
const VOICES = {
  ko: {
    languageCode: 'ko-KR',
    female: list('VOICE_KO_FEMALE', 'ko-KR-Chirp3-HD-Kore,ko-KR-Neural2-A'),
    male: list('VOICE_KO_MALE', 'ko-KR-Chirp3-HD-Charon,ko-KR-Neural2-C'),
  },
  en: {
    languageCode: 'en-US',
    female: list('VOICE_EN_FEMALE', 'en-US-Chirp3-HD-Kore,en-US-Neural2-F'),
    male: list('VOICE_EN_MALE', 'en-US-Chirp3-HD-Charon,en-US-Neural2-D'),
  },
};

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
};
const PUBLIC_DIR = path.join(__dirname, 'public');

function sendJson(res, status, body) {
  res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store' });
  res.end(JSON.stringify(body));
}

function readJson(req) {
  return new Promise((resolve, reject) => {
    let size = 0;
    const chunks = [];
    req.on('data', (c) => {
      size += c.length;
      if (size > 64 * 1024) {
        reject(Object.assign(new Error('요청이 너무 큽니다.'), { status: 413 }));
        req.destroy();
        return;
      }
      chunks.push(c);
    });
    req.on('end', () => {
      try {
        resolve(JSON.parse(Buffer.concat(chunks).toString('utf8') || '{}'));
      } catch {
        reject(Object.assign(new Error('잘못된 요청 형식입니다.'), { status: 400 }));
      }
    });
    req.on('error', reject);
  });
}

function cleanText(value) {
  const text = typeof value === 'string' ? value.trim() : '';
  if (!text) throw Object.assign(new Error('읽을 문장을 입력해 주세요.'), { status: 400 });
  if (text.length > MAX_TEXT) throw Object.assign(new Error(`한 번에 ${MAX_TEXT}자까지 입력할 수 있습니다.`), { status: 400 });
  return text;
}

async function callGoogle(url, body) {
  if (!API_KEY) {
    throw Object.assign(new Error('서버에 GOOGLE_API_KEY 가 설정되지 않았습니다. 관리자에게 문의해 주세요.'), { status: 503 });
  }
  let r;
  try {
    r = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-Goog-Api-Key': API_KEY },
      body: JSON.stringify(body),
      signal: AbortSignal.timeout(20000),
    });
  } catch (e) {
    throw Object.assign(new Error('음성 서비스에 연결하지 못했습니다. 인터넷 연결을 확인해 주세요.'), { status: 502, cause: e });
  }
  const data = await r.json().catch(() => ({}));
  if (!r.ok) {
    const detail = data?.error?.message || `HTTP ${r.status}`;
    throw Object.assign(new Error(`Google 서비스 오류: ${detail}`), { status: 502 });
  }
  return data;
}

async function handleSpeak(req, res) {
  const body = await readJson(req);
  const text = cleanText(body.text);
  const lang = body.lang === 'en' ? 'en' : 'ko';
  const gender = body.gender === 'male' ? 'male' : 'female';
  const set = VOICES[lang];

  let lastError;
  for (const name of set[gender]) {
    try {
      const data = await callGoogle(TTS_URL, {
        input: { text },
        voice: { languageCode: set.languageCode, name },
        audioConfig: { audioEncoding: 'MP3' },
      });
      if (!data.audioContent) throw new Error('음성 데이터가 비어 있습니다.');
      return sendJson(res, 200, { audio: data.audioContent, mime: 'audio/mpeg', voice: name });
    } catch (e) {
      lastError = e;
      if (e.status === 503) break; // 키 없음: 다른 목소리를 시도해도 소용없음
      console.warn(`[speak] ${name} 실패: ${e.message}`);
    }
  }
  throw lastError;
}

async function handleTranslate(req, res) {
  const body = await readJson(req);
  const text = cleanText(body.text);
  const target = body.target === 'ko' ? 'ko' : 'en';
  const data = await callGoogle(TRANSLATE_URL, { q: text, target, format: 'text' });
  const t = data?.data?.translations?.[0];
  if (!t) throw Object.assign(new Error('번역 결과가 비어 있습니다.'), { status: 502 });
  sendJson(res, 200, { text: t.translatedText, source: t.detectedSourceLanguage || null, target });
}

function serveStatic(req, res, pathname) {
  const rel = pathname === '/' ? 'index.html' : decodeURIComponent(pathname).replace(/^\/+/, '');
  const file = path.resolve(PUBLIC_DIR, rel);
  if (file !== PUBLIC_DIR && !file.startsWith(PUBLIC_DIR + path.sep)) {
    res.writeHead(403).end();
    return;
  }
  fs.readFile(file, (err, data) => {
    if (err) {
      res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' }).end('찾을 수 없습니다.');
      return;
    }
    res.writeHead(200, {
      'Content-Type': MIME[path.extname(file)] || 'application/octet-stream',
      'Cache-Control': 'no-cache',
    });
    res.end(data);
  });
}

const server = http.createServer(async (req, res) => {
  const { pathname } = new URL(req.url, 'http://localhost');
  try {
    if (req.method === 'POST' && pathname === '/api/speak') return await handleSpeak(req, res);
    if (req.method === 'POST' && pathname === '/api/translate') return await handleTranslate(req, res);
    if (req.method === 'GET' && pathname === '/api/status') return sendJson(res, 200, { ready: Boolean(API_KEY) });
    if (req.method === 'GET' || req.method === 'HEAD') return serveStatic(req, res, pathname);
    res.writeHead(405).end();
  } catch (e) {
    const status = e.status || 500;
    if (status >= 500) console.error(`[${pathname}]`, e.message);
    if (!res.headersSent) sendJson(res, status, { error: e.message || '알 수 없는 오류가 발생했습니다.' });
  }
});

server.listen(PORT, HOST, () => {
  console.log(`배리어프리 음성 안내 서버 실행 중: http://localhost:${PORT}`);
  if (!API_KEY) console.warn('주의: GOOGLE_API_KEY 가 설정되지 않아 읽어주기/번역이 동작하지 않습니다.');
});
