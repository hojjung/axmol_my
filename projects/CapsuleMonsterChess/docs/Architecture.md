# 프로젝트 아키텍처

## 결정

Firebase와 같은 Google Cloud 프로젝트·리전에 **private Cloud Run C++ 전투 서버**를 둔다.
현재 요구에는 Elastic Beanstalk나 EC2보다 운영 경계가 작다. 전투 전체를 서버에서 즉시 계산해
리플레이 이벤트를 반환하므로 상주 매치 서버와 WebSocket은 필요 없다.

```text
Axmol Client
  ├─ Firebase Hosting/CDN: WebAssembly, version manifest, bootstrap 콘텐츠
  ├─ Firebase Auth: 로그인과 ID token
  └─ Callable Function: Auth/App Check/버전/요청 검증
       ├─ Firestore: 프로필, 인벤토리, 덱, 매치, MMR, 원장
       └─ IAM OIDC → Private Cloud Run C++
                       └─ cmc::game_core
```

한국 우선 배포 시 Firestore, Functions, Cloud Run을 `asia-northeast3`에 맞춘다. Firestore 위치는
생성 후 바꿀 수 없으므로 실제 프로젝트 생성 전에 최종 확인한다. Cloud Run은 초기
`concurrency=1`, `min-instances=0`으로 시작한다.

pthread WebAssembly는 cross-origin isolation이 필수다. Firebase Hosting에
`Cross-Origin-Opener-Policy: same-origin`, `Cross-Origin-Embedder-Policy: require-corp`를 설정하고,
다른 도메인의 Storage/CDN 자산에는 CORP 또는 CORS를 설정한다. 초기 `.wasm/.data`와
`version.json`은 같은 Hosting origin에 둔다.

## 저장소 경계

```text
CapsuleMonsterChess/
  Source/                 Axmol 클라이언트, UI, 렌더, 실행/네트워크 어댑터
  GameCore/               순수 결정적 C++17 전투 규칙
  Server/                 headless HTTP/Cloud Run 어댑터
  Backend/                Firebase Functions와 보안 규칙
  Content/Data/Tables/    클라이언트·서버 공용 canonical JSON
  Tests/GameCore/         결정성·육각 보드·Unity golden tests
  docs/                   기획, 포팅, 마일스톤
```

`GameCore`는 Axmol, HTTP, Firebase, 파일 시스템을 알지 못한다. ExaStudio에서는 Boost.Asio/Beast,
nlohmann JSON, BS thread pool, mbedTLS, FlatBuffers를 필요한 것만 독립 타깃으로 가져올 수 있다.
기존 `exaNetwork` 전체는 엔진 의존성과 미완성 서버 경로가 있으므로 링크하지 않는다. 첫 버전은
JSON + HTTPS만 사용하고 FlatBuffers는 측정 후 도입한다.

Firebase C++ SDK는 WebAssembly를 지원하지 않는다. Web은 modular Firebase JavaScript SDK로 Auth,
App Check, callable을 처리하고 얇은 C ABI/`EM_JS` bridge로 C++에 결과만 넘긴다. Android/iOS는
Firebase C++ SDK adapter를 사용한다. 상위 코드는 `IIdentityService`와 `IBackendClient`만 보며,
두 adapter 모두 `Source/Platform` 아래에 둔다.

## 전투 실행 경계

실행 모드는 `Source/Client`의 coordinator 입력이고 simulator 내부 규칙은 아니다.

```cpp
enum class BattleExecutionMode : std::uint8_t
{
    Local,
    AuthoritativeRemote,
};

BattleHandle BattleCoordinator::Start(
    const BattleRequest& request,
    BattleExecutionMode mode);

BattleResult BattleSimulator::Run(
    const BattleSnapshot& snapshot,
    const BattleRules& rules,
    std::uint32_t seed);
```

- `Local`: 스토리와 오프라인 개발용. 같은 프로세스의 GameCore를 호출한다.
- `AuthoritativeRemote`: PvP용. 서버 결과와 리플레이만 사용한다.
- Local 결과는 PvP MMR, 토큰, 유료 재화를 절대 변경하지 못한다.

결정성을 위해 fixed tick, stable iteration order, 명시적 RNG를 사용하고 wall clock과 전역 상태를
금지한다. 전투 수치는 정수 또는 명시적 fixed-point로 계산하고, hash 입력은 endian과 필드 순서를
고정한 canonical bytes로 직렬화한다. 결과에는 `matchId`, `rulesVersion`, `contentHash`, `seed`,
`resultHash`를 기록한다.

## 서버 판정 흐름

1. 로그인 전에는 Hosting의 `version.json`으로 업데이트 필요 여부만 표시하고, 로그인 후 callable이
   ID token, App Check, 클라이언트 버전을 다시 강제한다.
2. 클라이언트가 재시도용 `requestId`를 보내면 서버가 Firebase UID와 묶어 idempotency key를 만든다.
3. 첫 Firestore transaction에서 해당 key의 match를 create-if-absent 하고 실제 덱·레벨·장비를
   읽어 입장권/피로도를 차감한 뒤, 불변 snapshot과 서버 seed를 가진 `Pending` match를 만든다.
4. private Cloud Run이 서버 테이블로 능력치를 조립하고 GameCore로 전투 전체를 계산한다.
5. 두 번째 transaction이 아직 `Pending`인 같은 match에만 결과·MMR·보상을 반영한다.
6. 호출 실패 시 같은 `requestId`를 재시도하고, 클라이언트는 확정된 이벤트만 화면 시간에 맞춰 재생한다.

클라이언트가 보낸 stats, skills, seed, 승패, 보상량은 신뢰하지 않는다. 민감한 Firestore 컬렉션은
클라이언트 직접 쓰기를 차단한다. `uid + requestId`와 상태 전이가 재시도나 중복 탭 요청의 이중
차감·이중 보상을 막는다.

## 콘텐츠 패키징

첫 실행에 필요한 폰트, 타이틀, Dragon fixture, 테이블 manifest만 bootstrap `.data`에 넣는다.
대형 모델·텍스처는 fingerprint가 붙은 remote asset manifest와 로컬 캐시로 분리한다. Axmol 기본
`Content/` 전체 preload는 마일스톤 1까지만 사용하고, 실제 CDN 다운로드를 붙일 때 bootstrap과
remote content의 CMake 패키징 목록을 분리한다.

HTML과 `version.json`은 `Cache-Control: no-cache`로 재검증한다. wasm/data/remote asset은 파일명에
content hash를 붙인 뒤에만 `public, max-age=31536000, immutable`을 사용한다. 현재처럼 파일명이
고정된 개발 빌드는 immutable 캐시를 사용하지 않는다.

## 플랫폼 선택

- 현재: Firebase Hosting + Auth + Firestore + Callable Functions + private Cloud Run.
- AWS가 필수가 되면 Elastic Beanstalk/직접 EC2 대신 ECS/Fargate를 검토한다.
- 블록체인은 `IWalletVerifier`, `ISettlementProvider` 같은 경계만 나중에 추가한다.

## 공식 근거

- [Cloud Run 개요](https://docs.cloud.google.com/run/docs/overview/what-is-cloud-run)
- [Firebase callable 인증 정보](https://firebase.google.com/docs/functions/callable)
- [Firebase C++ 지원 플랫폼](https://firebase.google.com/docs/cpp/learn-more)
- [Firebase Web SDK](https://firebase.google.com/docs/web/learn-more)
- [Cloud Run 서비스 간 인증](https://docs.cloud.google.com/run/docs/authenticating/service-to-service)
- [Firestore transaction](https://firebase.google.com/docs/firestore/manage-data/transactions)
- [Firebase Hosting CDN](https://firebase.google.com/docs/hosting/quickstart)
- [Firebase Hosting 헤더 설정](https://firebase.google.com/docs/hosting/full-config#headers)
- [Emscripten pthread 배포 요구사항](https://emscripten.org/docs/porting/pthreads.html)
- [Firestore 위치](https://firebase.google.com/docs/firestore/locations)
- [Amazon ECS 서비스](https://docs.aws.amazon.com/AmazonECS/latest/developerguide/ecs_services.html)
