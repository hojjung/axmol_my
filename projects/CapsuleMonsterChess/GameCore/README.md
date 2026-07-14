# GameCore

클라이언트와 C++ 전투 서버가 같은 소스와 규칙을 사용하는 순수 C++17 라이브러리다.

허용 범위:

- 육각 좌표, 배치, 이동, 타깃 선택
- 고정 틱 전투, 결정적 RNG, 상태효과
- 전투 요청/결과/리플레이 이벤트 자료형
- JSON 테이블을 해석한 불변 전투 규칙

금지 범위:

- Axmol, 렌더러, UI, HTTP, Firebase 의존성
- wall clock, 전역 가변 상태, 파일 I/O
- 컨테이너 순회 순서에 의존하는 판정

스토리의 `Local`과 PvP의 `AuthoritativeRemote` 구분은 `Source/Client`의 실행 어댑터에만 두며,
양쪽 모두 최종적으로 같은 `BattleSimulator::Run(snapshot, rules, seed)`를 호출한다.
