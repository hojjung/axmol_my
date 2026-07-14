# Milestone 01 — Dragon 10 대 10

## 목표

720×1280 세로 앱에서 Dragon 10마리와 더미 상대 Dragon 10마리가 10×5 육각 보드 위에서
이동·기본 공격하고, 같은 C++ 전투 코어가 로컬 또는 서버에서 결과와 리플레이를 만든다.

## 포함

- 10×5 육각 좌표, 이웃, 거리, 점유와 셀 예약
- 배치 검증, 타깃 선택, A* 이동, 공격 사거리 정지
- fixed tick, 기본 공격, 피해, 사망, 승패, 최대 tick 무승부
- Dragon 10 대 10 고정 테스트 덱과 서버 더미 상대
- Local/AuthoritativeRemote 단일 실행 모드 파라미터
- 서버 결과·이벤트 로그를 클라이언트가 재생하는 경로
- 동일 입력·seed의 result/replay hash 결정성 검증
- 클라이언트와 서버의 동일 `contentHash` 확인

## 제외

- 스킬, 이펙트, 장비, 진화, 인벤토리 UI
- 실제 Firebase 로그인, 매칭, MMR, 보상 반영
- 블록체인, 지갑, 토큰 정산
- WebSocket, Redis, 대규모 서버 인프라

## 완료 조건

1. macOS와 WebGL2에서 세로 화면이 정상 표시된다.
2. 20개 유닛이 겹치지 않고 유효한 hex에 배치된다.
3. 두 번 실행한 전투의 이벤트 순서, 종료 tick, 승자와 hash가 같다.
4. Local과 서버가 같은 fixture에서 같은 결과를 만든다.
5. PvP 원격 요청은 유닛 ID·배치만 받고 서버 테이블로 능력치를 구성한다.
6. 서버 응답을 재생한 최종 클라이언트 위치·HP·생존 상태가 결과와 일치한다.

## 구현 순서

1. Unity 라이브 Sheet 168행을 버전·hash가 있는 JSON fixture로 고정한다.
2. enum/DTO, RNG, checksum, hex board를 GameCore로 포팅한다.
3. Dragon 기본 스탯만으로 로컬 10 대 10 golden test를 통과시킨다.
4. headless 서버가 같은 fixture를 계산하도록 연결한다.
5. Axmol 씬에 hex 배치와 이벤트 로그 재생을 연결한다.
