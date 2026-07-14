# Server

private Cloud Run에서 실행할 headless C++ 어댑터 위치다. 첫 서버 API는 동기식
`POST /v1/battles:simulate` 하나로 시작한다.

- Firebase Function이 검증한 내부 요청만 받는다.
- 유닛 ID와 배치를 입력받고 능력치·스킬은 서버 보유 테이블로 조립한다.
- `cmc::game_core`로 전투 전체를 즉시 계산하고 결과와 리플레이 이벤트를 반환한다.
- 렌더러, Axmol scene, 클라이언트 저장 데이터는 링크하지 않는다.

첫 마일스톤에는 WebSocket, 상주 매치 룸, Redis, mTLS를 추가하지 않는다.
