# Backend

Firebase 공개 API 경계다. Cloud Functions for Firebase 2nd gen TypeScript를 배치한다.

초기 책임은 `bootstrap`, 버전 검사, Firebase Auth/App Check 검증, Firestore transaction,
private Cloud Run 호출뿐이다. 전투 판정은 C++ `GameCore`, 민감한 상태 변경은 서버만 담당한다.

로그인 전에는 Hosting의 `version.json`만 읽고, 로그인 후 callable이 버전을 다시 강제한다.
pthread WebAssembly 배포에는 Hosting의 COOP `same-origin`, COEP `require-corp` 헤더가 필수다.
교차 출처 콘텐츠를 추가할 때는 해당 Storage/CDN의 CORP 또는 CORS도 함께 설정한다.
HTML과 `version.json`은 `Cache-Control: no-cache`, fingerprint가 붙은 wasm/data/asset만
`public, max-age=31536000, immutable`로 배포한다.

블록체인 코드는 아직 넣지 않는다. 향후 지갑 연결과 주간 정산은 체인 독립 인터페이스로
분리하며 개인키는 Secret Manager/KMS 경계에 둔다.
