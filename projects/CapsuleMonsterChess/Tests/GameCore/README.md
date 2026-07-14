# GameCore Tests

첫 테스트 세트는 Unity golden fixture와 C++ 결과를 비교한다.

- 동일 입력과 seed를 두 번 실행한 replay/result hash 일치
- 10×5 hex 이웃, 거리, 점유, 셀 예약, A* 경로
- 원거리 유닛이 사거리 칸에서 정지하는지 검증
- 10 대 10 기본 공격 전투의 승자·종료 tick·이벤트 순서
- Local과 AuthoritativeRemote가 같은 스냅샷에서 같은 판정 생성

네트워크와 렌더링 테스트는 이 디렉터리에 넣지 않는다.
