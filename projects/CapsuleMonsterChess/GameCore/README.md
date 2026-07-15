# GameCore

클라이언트와 C++ 전투 서버가 같은 소스와 규칙을 사용하는 순수 C++17 라이브러리다.

허용 범위:

- 육각 좌표, 배치, 이동, 타깃 선택
- 고정 틱 전투, 결정적 RNG, 상태효과
- 전투 요청/결과/리플레이 이벤트 자료형
- JSON 테이블을 해석한 불변 전투 규칙

GameCore 자체에서 사용하지 않는 범위:

- Axmol, 렌더러, UI, HTTP, Firebase 의존성
- wall clock, 전역 가변 상태, 파일 I/O
- 컨테이너 순회 순서에 의존하는 판정

스토리의 `Local`과 PvP의 `AuthoritativeRemote` 구분은 `Source/Client`의 실행 어댑터에만 두며,
양쪽 모두 최종적으로 같은 `BattleSimulator::simulate(request)`를 호출한다.

서버 실행 파일은 Axmol CMake 타깃으로 만들고 `axmol`을 링크해도 된다. 이 분리는 서버에서 Axmol을
금지하기 위한 것이 아니라, 동일 전투 입력이 클라이언트·서버·단위 테스트에서 프레임 시간과 무관한
결과를 내도록 판정 코어만 독립시킨 것이다.

`validateBattleRequest()`는 시뮬레이션 전에 팀, 런타임 유닛 ID, 보드 용량, 틱과 수치 상한을 검사한다.
실패 결과는 `validationError != None`이며 checksum, 이벤트, 최종 상태를 만들지 않는다.
