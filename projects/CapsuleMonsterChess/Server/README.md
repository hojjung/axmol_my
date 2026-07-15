# Server

private Cloud Run에서 실행하는 headless C++ 전투 서버다. 전체 `axmol` 라이브러리를 링크하지 않고
`Scheduler`, fixed-priority `CustomEvent`, 객체 수명 관리에 필요한 소스만 `cmc_axmol_headless`로 빌드한다.
`Director`, `Scene`, Renderer/RHI, UI, 입력, 물리, 오디오, 네트워크 모듈은 서버 타깃에 포함하지 않는다.
헤드리스 `EventDispatcher`에서는 Node/SceneGraph 우선순위 API와 상태도 컴파일하지 않는다.

## 실행 경계

- `POST /v1/battles:simulate`: 서버 테이블로 능력치를 조립하고 `cmc::game_core`로 전투를 즉시 계산한다.
- `GET /healthz`: 테이블 버전, 카탈로그 수, Axmol Scheduler/EventDispatcher 동작 상태를 반환한다.
- Axmol `Scheduler`는 프로세스 런타임의 단일 update selector에만 사용한다.
- 프로세스 내부 완료 알림은 Axmol `EventDispatcher`의 fixed-priority `CustomEvent` 경로를 사용한다.
- Scheduler와 EventDispatcher는 하나의 Boost.Asio strand에서만 접근한다.
- 결정적 simulation tick, 동일 tick 내 순서, 리플레이 로그와 checksum은 C++17 `cmc::game_core`가 소유한다.

Cloud Run 서비스는 IAM 인증을 강제하고 Firebase Function의 서비스 계정만 호출할 수 있게 배포한다.
Function이 Auth, App Check, 클라이언트 버전과 Firestore 덱을 검증하고 클라이언트 seed를 폐기한 뒤 서버
seed를 생성한다. C++ 서버는 신뢰 경계를 지난 `uid`, 유닛 ID, 배치와 seed만 받고, 능력치는 서버가 가진
canonical JSON에서 읽는다. MMR·보상·재화 쓰기는 Function의 Firestore transaction에서 확정한다.

Cloud Run이 외부 HTTPS와 TLS를 종료하므로 컨테이너는 `0.0.0.0:$PORT`에서 HTTP를 수신한다. 첫
마일스톤에는 WebSocket, 상주 매치 룸, Redis, 애플리케이션 mTLS를 추가하지 않는다.

## 빌드와 테스트

현재 Axmol 헤더 요구사항 때문에 서버 어댑터는 C++23으로 컴파일하며, 공유 전투 규칙인 GameCore는
C++17을 유지한다.

```sh
cd projects/CapsuleMonsterChess
cmake -S Server -B build_server -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_server -j 8
ctest --test-dir build_server --output-on-failure
PORT=8080 ./build_server/bin/cmc_battle_server
```

환경 변수:

- `PORT`: 수신 포트, 기본값 `8080`
- `CMC_SERVER_THREADS`: Asio worker 수, 범위 `1..32`
- `CMC_TABLE_PATH`: `monster_unit_table.json` 경로

컴파일된 기본 테이블 경로는 로컬 개발 편의용이다. Cloud Run 이미지에서는 JSON을 이미지 내부에 복사하고
`CMC_TABLE_PATH=/app/data/monster_unit_table.json`처럼 반드시 명시한다.

전투 요청 schema 1은 `requestId`, `battleId`, `uid`, 서버 생성 `seed`, `tableVersion`, `contentHash`,
`playerUnits`, `enemyUnits`만 허용한다. 각 유닛은 `runtimeUnitId`, `catalogUnitId`, `x`, `z`만 보낼 수
있으며 HP·공격력·스킬·승패·보상 필드는 거부한다. 실행 가능한 10대10 예시는
[examples/dragon_10v10.json](examples/dragon_10v10.json)에 있다.

ExaStudio에서는 Boost 1.90의 header-only Asio/Beast, nlohmann JSON 단일 헤더, Mbed TLS의 SHA-256
소스만 사용한다. `exaNetwork`, BS thread pool, curl, 전체 TLS 라이브러리는 링크하지 않는다. 세부
라이선스 경로는 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 기록한다.
