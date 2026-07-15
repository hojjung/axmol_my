# Capsule Monster Chess

세로형 Axmol 멀티플랫폼 프로젝트 기반이다. 현재 첫 씬은 `Dragon Fire@Fly Idle.FBX`를
호스트에서 GLB로 변환해 Stylized Forward Renderer로 표시한다. 재질은 Unity의
JMO ramp/diffuse tint와 AC directional rim/SSS 특성을 런타임 파라미터로 재구현하며,
원본 유료 셰이더 코드는 포함하지 않는다.

## 프로젝트 경계

- `Source`: Axmol 클라이언트, UI, 렌더링과 네트워크 어댑터
- `GameCore`: 클라이언트·서버 공용 순수 C++17 전투 코어
- `Server`: private Cloud Run용 headless C++ 어댑터
- `Backend`: Firebase Auth/Firestore/Functions 공개 API 경계
- `Content/Data/Tables`: 클라이언트·서버 공용 canonical JSON
- `Tests/GameCore`: 결정성, 육각 보드, Unity golden tests

기획과 구현 순서는 [GameDesign](docs/GameDesign.md), [Architecture](docs/Architecture.md),
[Milestone 01](docs/Milestone01.md), [Unity Port Map](docs/UnityPortMap.md)을 기준으로 한다. 헤드리스 빌드와
HTTP 계약은 [Server](Server/README.md), 클라이언트·서버 모듈 경계와 삭제 목록은
[Axmol Runtime Module Profile](docs/AxmolModuleProfile.md)에 있다. Axmol UI, Director, 게임 로직과
Unity·Unreal 대응 관계를 포함한 수동 구현 기준은
[수동 코딩 인수인계](../../docs/CapsuleMonsterChessManualCodingHandoff.md)를 따른다. Unity 원본
프리팹·씬·C# UI 프레임워크와 Anchor, Stretch, Layout, 이벤트, 애니메이션, Gradient, Vectrosity,
번역을 항목별로 옮기는 방법은
[Unity UI → Axmol 구현 가이드](../../docs/CapsuleMonsterChessUnityUiPortingGuide.md)에 정리되어 있다.

## Axmol 모듈 경계

클라이언트 기능 스위치는
[`cmake/modules/AXGameEngineOptions.cmake`](cmake/modules/AXGameEngineOptions.cmake)가 단일 기준이다.
코어 2D UI, 3D/glTF, KTX2/BasisU와 HTTP/Downloader만 유지하고 Box2D, Jolt, Recast, WebSocket,
영상, 오디오와 미사용 확장을 빌드에서 제외한다. 사운드 구현을 시작할 때만
`-DCMC_ENABLE_AUDIO=ON`으로 오디오 모듈을 다시 켠다.

모바일 텍스처는 KTX2/Basis Universal 한 벌을 배포하고, 런타임에서 ASTC 우선, ETC2·BC3·RGBA8
순으로 기기 지원 포맷에 전사한다. 정확한 유지·삭제 목록과 `AX_WITH_ASTCENC` 예외는
[Axmol Runtime Module Profile](docs/AxmolModuleProfile.md)을 따른다.

서버는 전체 Axmol target을 링크하지 않는다. `Server/CMakeLists.txt`의 allowlist에 있는 Scheduler,
fixed-priority CustomEvent와 객체 수명 소스만 직접 컴파일한다.

## 로컬 자산 변환

Unity 원본과 변환 결과는 라이선스 자산이므로 Git에 커밋하지 않는다.
`axasset`과 `gltfpack`은 엔진 루트의 로컬 도구이며 변환 결과만 프로젝트에서 사용한다.

```sh
mkdir -p Content/Local

../../build-axasset/tools/axasset/axasset \
  --output Content/Local/DragonFireFlyIdle.glb \
  '/Users/ethanjung/Desktop/Dev/Unity/CapsuleMonsterChess/CMS_Unity/Assets/Models/Unit02/DragonFire/FBX/Dragon Fire@Fly Idle.FBX'
```

결과 런타임 포맷은 meshopt 압축과 KTX2 Basis 텍스처를 포함한 self-contained GLB다.

## macOS OpenGL 빌드

```sh
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer \
AX_ROOT=/Users/ethanjung/Desktop/Dev/Cpp/axmol_my \
  ../../tools/cmdline/axmol build -p osx -a arm64 -f \
  -xc '-DAX_RENDER_API=gl'
```

앱 번들은 `build/build/RelWithDebInfo/CapsuleMonsterChess.app`에 생성된다.

## WebGL2 릴리스 프로파일

```sh
source ../../tools/external/emsdk/emsdk_env.sh

AX_ROOT=/Users/ethanjung/Desktop/Dev/Cpp/axmol_my \
  ../../tools/cmdline/axmol build -p wasm -f \
  -xc '-DAX_PROFILE_STYLIZED_WEB=ON' \
  -xb '--config','Release'
```

## 로컬 브라우저 실행

pthread 빌드이므로 `file://`이나 일반 정적 서버가 아니라 COOP/COEP 헤더를 제공해야 한다.
Emscripten의 `emrun`은 해당 헤더를 포함한다.

```sh
source ../../tools/external/emsdk/emsdk_env.sh

cd build_wasm/bin/CapsuleMonsterChess
emrun --no_browser --port 8765 CapsuleMonsterChess.html
```

브라우저에서 `http://127.0.0.1:8765/CapsuleMonsterChess.html`을 연다.
