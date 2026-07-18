# Unity 포팅 맵

조사 기준은 Unity 저장소 `main`의 `fb64d6cd`다. `UNITY_ROOT`는
`/Users/ethanjung/Desktop/Dev/Unity/CapsuleMonsterChess`를 뜻한다.

## 인수인계 문서

- `AGENTS.md`: 세로형, 10×5 hex, 6개 체스 역할군, 5속성, 3단 진화, 비동기 PvP 기획.
- `Docs/CMS_Handoff_2026-05-07.md`: Unity/공유 C#/Cloud Run .NET/Firebase 구현 상태.
- `Docs/다음할일0507.txt`: Addressables, 풀링, 소켓 파티클, 익스트랙션 메모.

과거 문서의 Unity/C# 서버와 코인 제외 결정은 현재 지시로 대체한다. 게임 규칙과 이미 검증된
전투 동작은 보존하고 실행 기반만 Axmol/C++ 공용 코어로 옮긴다.

## 테이블 현황

Unity 저장소에는 CMS 전용 런타임 JSON이 없다.

```text
Google Sheet CSV
  → SpreadsheetUnitTableImporter
  → MonsterUnitTable ScriptableObject
  → Editor 수동 JSON Export
```

관련 소스:

- `CMS_Unity/Assets/Scripts/GameShared/Tables/MasterDataDtos.cs`
- `CMS_Unity/Assets/Scripts/GameShared/Tables/SpreadsheetUnitTableImporter.cs`
- `CMS_Unity/Assets/Scripts/Tables/MonsterUnitTableAsset.cs`
- `CMS_Unity/Assets/Scripts/Editor/MonsterUnitTableAssetEditor.cs`

Axmol 프로젝트에는 이 경로를 우회해 라이브 Sheet와 Unity 썸네일을 직접 가져오는
`Tools/import_unity_content.py`를 추가했다.

```sh
cd /Users/ethanjung/Desktop/Dev/Cpp/axmol_my/projects/CapsuleMonsterChess
python3 Tools/import_unity_content.py --unity-root "$UNITY_ROOT"
```

결과는 `Content/Data/Tables/monster_unit_table.json`,
`Content/Sprites/Units/<NameKey>/<NameKey>_1..3.png`,
`Content/Models/Units/<NameKey>/<NameKey>.glb`에 생성된다. 구매한 원본 에셋에서 생성한
런타임 PNG와 GLB는 CapsuleMonsterChess 프로젝트 자산으로 함께 추적한다.

라이브 Sheet는 유효 유닛 168개다. 역할별로 Pawn 42, Knight/Bishop/Rook 각 28, Queen 24,
King 18이며 스킬 1~3은 비어 있다. `Resources/MonsterUnitTable.asset`은 2026-07-04 스냅샷으로
현재 Sheet와 RNG 70개가 다르고, `Resources/Tables/MonsterUnitTable.asset`은 빈 중복 에셋이다.
현재 Export는 `Characters`만 기록해 version/hash/source가 없다.

현재 임포터는 `.asset`을 복사하지 않고 168개 유닛을 `NameKey` 논리 키 기준으로
가져온다. 이미지 판독으로 확정해 Unity `UnitThumbnails`의 파일명을 `NameKey_1..3`으로
정리한 145종은 아이콘과 모델을 모두 연결한다. 원본 썸네일과 모델 자체가 없는 23종은
빈 경로를 유지하며 로비에서 속성색과 `NameKey` 모노그램 placeholder로 표시한다.

현재 로비용 JSON은 `schemaVersion`, 원본 URL/hash, 경고, 아이콘 바인딩 결과와
`characters`를 기록한다. 서버 권위 전투 배포 전에 `tableVersion`, `sourceRevision`,
`contentHash`를 포함한 canonical 스냅샷으로 고정하고 같은 파일을 클라이언트와 서버에 넣는다.

Unity 저장소와 연결된 Sheet 및 전체 Git 이력에는 스토리 스테이지 마스터가 없다.
`WidgetMain_Stage.prefab`의 `Stage 3123`, `1/5`는 정적 표시 샘플이며 Battle 버튼에도 런타임
콜백이 없다. 따라서 Axmol의 `story_stage_table.json` 24개 항목은 선택 화면 검증용 bootstrap이며
`source.authoritative: false`다. 실제 밸런스 테이블이 확보되면 동일 스키마의 배포 스냅샷으로
교체한다. 잠금과 클리어 상태는 테이블이 아니라 사용자 진행 데이터에서 계산한다.

## 모델 현황

확정된 썸네일의 `UnitXX/<제작자 캐릭터명>` 폴더를 같은 위치의 `Models` 폴더와 대응시켜
145종을 self-contained GLB(meshopt + KTX2)로 변환했다. 프리뷰는 메시가 포함된 Idle FBX를
우선하고, 애니메이션 전용 FBX처럼 렌더 메시가 없으면 같은 캐릭터의 base FBX로 폴백한다.
현재 107종은 GLB 내 애니메이션을 재생하고 38종은 정적 모델로 표시된다. 모든 GLB는 메시와
유효한 헤더를 검증했으며 테이블의 `modelId`와 `icon`이 실제 파일 경로를 가리킨다.

## 현재 공유 전투 코어

| C# 원본 | C++ 대상 | 주의점 |
| --- | --- | --- |
| `Battle/BattleEnums.cs` | `GameCore` enum | numeric value를 fixture에 고정 |
| `Battle/BattleDtos.cs` | snapshot/result/event DTO | wire DTO와 내부 상태 분리 |
| `Battle/DeterministicRandom.cs` | 결정적 RNG | Unity golden vector 유지 |
| `Battle/HexBoard.cs` | hex/grid/path | 10×5, A*, 점유 규칙 |
| `Battle/BattleCommandQueue.cs` | tick command queue | stable ordering 명시 |
| `Battle/BattleSimulator.cs` | simulator | 50ms fixed tick |
| `Battle/BattleChecksum.cs` | replay/result hash | 기존 checksum은 결정성용일 뿐 보안 서명 아님 |
| `GameLogic/Battle/BattleSimulationClient.cs` | 실행 모드 adapter | Local/Remote만 분리 |
| `GameLogic/Battle/BattleReplayPlayer.cs` | Axmol replay adapter | 판정 없이 이벤트만 재생 |

Unity `BattleSimulationClient`와 `BattleReplayPlayer`는 현재 Scene/Prefab에 연결되지 않았다. 로그인,
인벤토리, SaveData, MainScene도 대부분 스텁이며 Firebase SDK/config와 실제 MMR·보상 API는 없다.
기존 RNG는 32-bit xorshift이므로 C++ 포팅도 `std::uint32_t` seed/state로 golden vector를 유지한다.

## 즉시 보안 조치

과거 커밋 `1b1d410c`의
`OldWebAppServer/myfirsttontest-firebase-adminsdk-fbsvc-013a628493.json`에 Firebase Admin 서비스 계정
개인키가 커밋돼 있었다. `846aa185`에서 파일을 지웠지만 Git 이력에서 복구 가능하므로 유출된 키로
간주해야 한다.

- GCP IAM에서 해당 service-account key를 즉시 revoke하고 새 키가 필요하면 교체한다.
- Git history 정리 여부와 무관하게 과거 키를 절대 재사용하지 않는다.
- 새 백엔드는 로컬 JSON 키를 커밋하지 않고 Cloud Run service account와 Secret Manager/KMS를 쓴다.
- 이 저장소나 로그에 과거 `private_key` 값을 다시 출력하지 않는다.

## 상용 원본

더 완성된 전투 코드는 `846aa185`에서 삭제되기 직전 커밋
`103228da65096382bbdac1450deaeeb598d42e6f`의 `Old_FTQ`에 있다.

- `Old_FTQ/Battle/HexGrid/HexGrid.cs`
- `Old_FTQ/Battle/JobMoveThread/Job_PathFind.cs`
- `Old_FTQ/Battle/JobMoveThread/Job_Manager.cs`
- `Old_FTQ/Battle/UnitActor_FSM.cs`
- `Old_FTQ/Battle/UnitManager.cs`
- `Old_FTQ/Battle/GAS/AbilityComponent.cs`
- `Old_FTQ/Battle/GAS/EffectComponent.cs`
- `Old_FTQ/Battle/UnitItemInventory.cs`
- `Old_FTQ/Battle/EvolutionManager.cs`
- `Old_FTQ/Battle/SlaySpireMap/MapManager.cs`

여기서 셀 예약, 근접/원거리 타깃 선택, GAS 트리거, 장비, 진화 규칙을 선별 이식한다.
UnityEngine, Job/Burst, `Time.deltaTime` 실행 코드는 복사하지 않고 판정 규칙만 fixed-tick C++로 옮긴다.

## 그대로 재현하지 않을 확인된 문제

- 현재 `HexBoard.TryFindNextStep`은 원거리 유닛도 목표 인접 칸까지 이동시킨다.
- `Weakness`가 공격력 감소가 아니라 대상 피격 피해 감소로 계산된다.
- `isBasicAttack` 미사용으로 스킬/AoE/DOT도 공격 에너지를 얻는다.
- DOT 발생자가 죽으면 남은 DOT가 중단된다.
- cooldown과 여러 GAS trigger/status는 enum만 있고 판정이 미완성이다.
- 현재 서버는 클라이언트가 보낸 stats, skills, seed와 tableVersion을 신뢰한다.

이 항목은 Unity golden test를 만들 때 의도된 규칙을 먼저 확정한다. 이미 성공시킨 Old_FTQ의 전투
감각은 유지하되, 명백한 결함과 비결정적 실행 방식은 C++ 기준으로 교정한다.

## 포팅 순서

1. 라이브 Sheet를 canonical JSON으로 고정한다.
2. enum/DTO/RNG/checksum/hex board를 순수 C++17로 옮긴다.
3. 서버는 유닛 ID와 배치만 받고 자체 테이블로 전투 스냅샷을 만든다.
4. Old_FTQ에서 셀 예약·타깃 선택·GAS 판정을 필요한 순서대로 이식한다.
5. 같은 simulator에 Local/AuthoritativeRemote adapter를 붙인다.
6. Dragon 10 대 10 서버 계산 → 이벤트 반환 → 세로 클라이언트 재생을 완성한다.
