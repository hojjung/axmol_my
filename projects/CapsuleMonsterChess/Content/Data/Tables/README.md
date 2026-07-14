# Tables

클라이언트와 서버가 함께 패키징하는 전투 테이블의 단일 기준 위치다.

Unity 원본은 현재 JSON 파일이 아니라 Google Sheet → CSV importer → ScriptableObject → 수동
JSON Export 흐름이다. 따라서 오래된 `.asset`을 복사하지 않고, 라이브 Sheet를 검증한 뒤 아래
메타데이터를 포함한 canonical JSON 스냅샷을 생성한다.

```json
{
  "schemaVersion": 1,
  "tableVersion": "YYYY.MM.DD.N",
  "sourceRevision": "sheet-revision",
  "contentHash": "sha256:...",
  "units": []
}
```

빌드와 서버 배포는 같은 `contentHash`를 사용한다. PvP 서버는 버전이 다른 요청을 거부하고,
클라이언트가 보낸 능력치·스킬·seed는 신뢰하지 않는다.

`contentHash`는 자기 자신을 제외한 객체를 key 정렬·UTF-8·공백 없음 규칙으로 canonicalize한 bytes의
SHA-256이다. 검증할 때도 `contentHash` 필드를 제거한 뒤 같은 방식으로 다시 계산한다.
