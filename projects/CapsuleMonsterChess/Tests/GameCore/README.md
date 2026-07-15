# GameCore Tests

첫 테스트 세트는 Unity golden fixture와 C++ 결과를 비교한다.

- 동일 입력과 seed를 두 번 실행한 replay/result hash 일치
- 10×5 hex 이웃, 거리, 점유, 셀 예약, A* 경로
- Unity smoke 및 10 대 10 전투의 승자, 종료 tick, 이벤트 checksum golden
- 빈 팀, 중복 ID, 보드 용량 초과, 비정상 입력 상한의 안전한 거절

네트워크와 렌더링 테스트는 이 디렉터리에 넣지 않는다.
