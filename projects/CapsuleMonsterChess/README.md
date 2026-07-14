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
[Milestone 01](docs/Milestone01.md), [Unity Port Map](docs/UnityPortMap.md)을 기준으로 한다.

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
