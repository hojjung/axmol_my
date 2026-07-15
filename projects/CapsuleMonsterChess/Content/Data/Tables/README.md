# Tables

클라이언트와 서버가 함께 패키징하는 전투 테이블의 단일 기준 위치다.

Unity 원본은 현재 JSON 파일이 아니라 Google Sheet → CSV importer → ScriptableObject → 수동
JSON Export 흐름이다. 오래된 `.asset`을 복사하지 않고 다음 명령으로 라이브 Sheet와
Unity 유닛 썸네일을 로비용 스냅샷으로 갱신한다.

```sh
cd projects/CapsuleMonsterChess
python3 Tools/import_unity_content.py \
  --unity-root /Users/ethanjung/Desktop/Dev/Unity/CapsuleMonsterChess
```

기본 아이콘 크기는 192×192이며 `--icon-size` 인자로 바꿀 수 있다. 실행 결과는
`monster_unit_table.json`과 `Content/UI/MonsterIcons/*.png`에 기록된다. PNG는 Unity 라이선스
파생 자산이므로 로컬 빌드에서만 사용하고 `.gitignore`로 공개 저장소 커밋을 차단한다.

현재 라이브 Sheet에서 168개 유닛을 가져온다. 영속 저장과 서버 payload는 숫자 `unitId`를
안정 키로 사용하고, 화면은 `NameKey`로 `_1` 촬영본 썸네일과 결합한다. `_2`, `_3`은 진화가
아닌 다른 촬영 각도이므로 카드 아이콘으로 가져오지 않는다. 현재 145개가 사진과 결합되며,
미해결 23개는 속성색·모노그램 placeholder로 표시된다.

현재 스냅샷은 `schemaVersion`, `tableVersion`, `contentHash`, `source`, `warnings`, `iconBinding`,
`characters`를 기록한다. `characters`가 서버 권위 전투 카탈로그이며 나머지 source/warning 정보는
가져오기 추적용이다.

```json
{
  "schemaVersion": 1,
  "tableVersion": "monster.v1.4ef396173938",
  "contentHash": "4ef3961739384773c044e9aead83170dfacde5ce4d4c4e21f22a89c8b431bc7f",
  "characters": []
}
```

빌드와 서버 배포는 같은 `contentHash`를 사용한다. PvP 서버는 버전이 다른 요청을 거부하고,
클라이언트가 보낸 능력치·스킬은 받지 않는다. Firebase Function은 클라이언트 seed를 폐기하고 서버에서
생성한 seed만 private Cloud Run 내부 요청에 넣는다.

`contentHash`는 정확히 `{schemaVersion, characters}` 객체를 key 정렬·UTF-8·공백 없음 규칙으로
직렬화한 bytes의 SHA-256 소문자 64자리 hex다. `tableVersion`은
`monster.v1.<contentHash 앞 12자리>`다. importer가 두 값을 함께 찍고, 전투 서버는 시작할 때 같은
canonical payload를 다시 해시해 파일 변조나 불완전 배포를 거부한다.

## 사용자 프로필

`Content/Data/Local/default_user_profile.json`은 테스트용 최초 프로필이다. 런타임의
`IUserDataSource`는 같은 `UserProfile` DTO에 다음 두 소스를 연결한다.

- `LocalJsonUserDataSource`: writable path에 JSON을 저장한다. WASM은 Axmol IDBFS에 flush한다.
- `WebPayloadUserDataSource`: 서버에서 받은 JSON payload를 읽으며 클라이언트 저장은 금지한다.

프로필 schema 2는 재화, 보유 유닛, 장비, 덱과 함께 `questProgress` 및 `battlePass`를 저장한다.
패스 수령 이력은 순차 수령을 가정하지 않고 `claimedFreeTierIds`, `claimedPremiumTierIds` 배열로
기록한다. schema 1 로컬 저장은 먼저 빈 진행 상태로 읽은 뒤, 로비가 현재 카탈로그의 신규 퀘스트를
진행도 0으로 추가하고 지난 시즌 패스를 초기화해 즉시 schema 2로 다시 저장한다. 이 보정은 로컬 테스트
소스에만 적용하며, 버전이 맞지 않는 웹 payload는 임의 보정하지 않고 거부한다.

`lobby_progression_table.json`은 Unity의 GUI Kit 화면을 연결하기 위한 비권위 개발 seed다. Unity
원본에는 퀘스트·배틀패스 마스터나 진행 저장 로직이 없다. 상용 환경에서는 서버 마스터와 사용자
payload가 기준이며, 퀘스트/패스 수령·가챠·구매·재화 변경은 클라이언트 save가 아니라 인증된 서버
명령 결과로만 갱신한다.

퀘스트 선행 조건은 배열 순서가 아니라 `prerequisiteQuestId`로 명시한다. 패스 티어는 UI의 레벨 계산과
서버 검증이 어긋나지 않도록 1부터 빈틈없이 오름차순이어야 하며 로드 시 이를 검증한다.

## 스토리 스테이지

`story_stage_table.json`은 로비의 스테이지 선택 UI를 검증하기 위한 24개 개발용 seed다. Unity
저장소와 연결된 Google Sheet에는 스테이지 마스터가 없고, 프리팹의 `Stage 3123`, `1/5` 값도
정적 UI 샘플이므로 Unity 원본 데이터로 간주하지 않는다. 이를 테이블의
`source.authoritative: false`로 고정했다.

테이블은 스테이지 ID, 챕터, 순서, 적 전투력, 소모 에너지, 골드 보상만 가진다. 클리어·잠금·별점은
사용자 진행 데이터이며 마스터 테이블에 저장하지 않는다. 로비는 대량 스테이지에서도 노드 수가
증가하지 않도록 한 페이지에 최대 20개를 4×5 그리드로 생성한다.
