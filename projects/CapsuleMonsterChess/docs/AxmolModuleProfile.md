# Axmol 런타임 모듈 프로파일

이 문서는 Capsule Monster Chess 클라이언트와 전투 서버에서 유지·제거한 Axmol 모듈의 기준이다.
실제 클라이언트 스위치의 단일 기준은
[`cmake/modules/AXGameEngineOptions.cmake`](../cmake/modules/AXGameEngineOptions.cmake), 서버 소스 경계의
단일 기준은 [`Server/CMakeLists.txt`](../Server/CMakeLists.txt)다.

## 텍스처 정책: KTX2/Basis Universal과 ASTC

모바일 GPU의 최종 압축 포맷은 ASTC가 우선이다. 다만 원본 자산을 플랫폼마다 ASTC·ETC2로 중복
보관하지 않고, KTX2 안의 Basis Universal 데이터를 공통 배포 포맷으로 사용한다.

Axmol의 `Image::initWithKTX2Data()`는 실행 기기의 GPU 지원을 확인한 뒤 다음 순서로 전사한다.

1. ASTC 지원 기기: `ASTC 4x4 RGBA`
2. ASTC 미지원, ETC2 지원 기기: `ETC2 RGBA`
3. 데스크톱 S3TC 지원 기기: `BC3/DXT5`
4. 압축 포맷 미지원: `RGBA8`

즉 Basis Universal이 ASTC 텍스처 사본을 내장한다는 뜻은 아니다. KTX2의 ETC1S/UASTC 중간 표현을
Basis Universal transcoder가 기기에 맞는 GPU 네이티브 포맷으로 로드 시점에 변환한다. 현재 파이프라인은
한 개의 KTX2/GLB 자산으로 iOS의 ASTC와 Android의 ASTC 또는 ETC2 fallback을 모두 처리한다.
단일 배포 자산의 대가로 최초 로드 때 CPU 전사와 임시 메모리 비용은 발생하지만, 전사 후 텍스처는
ASTC 또는 ETC2 GPU 네이티브 압축 상태로 업로드된다.

`AX_WITH_ASTCENC`는 이 KTX2 transcoder와 별개인 레거시 `.astc` CPU 디코더 링크 옵션이다. 현재
Axmol의 `Image.cpp`가 `AX_ENABLE_LEGACY_IMAGE_FORMATS=OFF`에서도 해당 심볼을 참조하므로 링크 안정성을
위해 예외적으로 유지한다. 이 옵션까지 제거하려면 먼저 레거시 ASTC 메서드와 include를
`AX_ENABLE_LEGACY_IMAGE_FORMATS` 경계로 분리해야 한다.

공식 근거:

- [Khronos KTX transcode 대상 포맷](https://github.khronos.org/KTX-Software/ktxtools/ktx_transcode.html)
- [Khronos KTX 2.0 규격](https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html)
- [Android Texture Compression Format Targeting](https://developer.android.com/guide/playcore/asset-delivery/texture-compression)
- [Apple Metal feature set tables](https://developer.apple.com/metal/feature-sets/)

## 클라이언트 유지 모듈

| 모듈 | 유지 이유 |
|---|---|
| Axmol core, 2D scene/UI | 세로형 로비, Sprite, Label, DrawNode, Action, 입력 |
| 3D, glTF | 유닛 GLB 모델과 전투 씬 렌더링 |
| KTX2, Basis Universal, meshoptimizer | 단일 경량 텍스처·메시 배포 경로 |
| HTTP, CURL, LLHTTP, Downloader | CDN, 로그인, 사용자 데이터, 전투 API |
| PNG, FreeType | UI 이미지와 글꼴 |
| Unzip, Clipper2, Poly2Tri | 현재 Axmol core 빌드의 필수 종속성 |
| ASTCENC | 위에 설명한 `Image.cpp` 링크 경계 예외 |

`2D scene/UI`와 `Physics2D`는 별개다. 2D UI는 유지하지만 충돌 판정에 쓰지 않는 Box2D는 제거한다.

## 클라이언트 제거 모듈

| 제거 기능 | 제거되는 구현/종속성 | 다시 켜는 조건 |
|---|---|---|
| Physics2D | Box2D | 실제 물리 충돌 게임플레이가 추가될 때 |
| Physics3D | Jolt | 서버와 동일한 결정론 전투가 아닌 물리 기능이 필요할 때 |
| NavMesh | Recast, FastLZ | 육각 보드 이외의 자유 이동 맵을 도입할 때 |
| Legacy 3D | 구형 3D 노드·로더 | 현재 glTF 경로로 표현할 수 없는 자산이 생길 때 |
| Audio | Axmol 오디오 엔진 | 사운드 구현 시작 시 `-DCMC_ENABLE_AUDIO=ON` |
| Opus | Opus codec | 실제 Opus 콘텐츠를 도입할 때 별도 활성화 |
| Video/Media | Video, MFMedia, VLC | 영상 재생 기능을 실제로 넣을 때 |
| WebSocket | WebSocket, parser, WASM `websocket.js` | 실시간 서버 push가 HTTP보다 명확히 필요할 때 |
| VR/XR | VR, OpenXR | 지원 플랫폼으로 확정될 때 |
| WebView | Edge WebView2 | 인게임 웹 UI가 필요할 때 |
| Legacy image | BMP, JPEG, WebP 및 레거시 감지 경로 | 콘텐츠 테이블에 해당 포맷이 들어올 때 |
| Test dependency | doctest | Axmol 자체 테스트 target을 빌드할 때 |
| Optional extensions | Lua, GUI extension, AssetManager, Spine, DragonBones, SceneIO/SceneExt, FairyGUI, ImGui, Live2D, Effekseer, Particle3D, PhysicsNode, Inspector, SDFGen, JSONDefault | 해당 기능의 런타임 소스와 자산이 함께 추가될 때 |

여기서 `GUI extension`은 코어 `axmol/ui`가 아니다. 로비와 게임 HUD에 쓰는 코어 UI는 유지한다.
확장을 다시 도입할 때는 `AX_EXT_HINT` 전체를 켜지 않고 필요한 개별 스위치만 활성화한다.

## 헤드리스 서버 경계

서버는 전체 Axmol target을 링크하지 않는다. 다음 9개 소스만 `cmc_axmol_headless`에 allowlist로
직접 컴파일한다.

- `AutoreleasePool.cpp`
- `CustomEvent.cpp`
- `CustomEventListener.cpp`
- `Event.cpp`
- `EventListener.cpp`
- `Object.cpp`
- `Scheduler.cpp`
- `WeakPtr.cpp`
- `Server/AxmolHeadless/EventDispatcherHeadless.cpp`

따라서 서버에서는 renderer/RHI, Director, Scene/Node, platform input, 2D·3D physics, navigation,
audio, video, Axmol network 모듈이 빌드되지 않는다. HTTP 서비스는 별도 Boost.Asio/Beast 경로를
사용한다. 서버가 Axmol에서 공유하는 것은 Scheduler,
fixed-priority CustomEvent/EventDispatcher, Object 수명 관리와 순수 C++ `GameCore`뿐이다.

## 변경 규칙

1. 새 기능의 런타임 코드와 자산이 실제로 들어오기 전에는 모듈을 미리 켜지 않는다.
2. 클라이언트 스위치는 `AXGameEngineOptions.cmake`에서만 변경한다.
3. 서버 Axmol 소스는 `Server/CMakeLists.txt` allowlist에 필요한 최소 파일만 추가한다.
4. 클라이언트와 서버의 전투 규칙은 `GameCore`에 한 번만 구현한다.
5. 모듈 변경 후 macOS, WebAssembly, GameCore test, server test를 함께 확인한다.

## 2026-07-15 검증 기준

- macOS RelWithDebInfo 클라이언트 빌드·실행 성공
- WebAssembly Release 빌드와 WebGL2 로컬 브라우저 smoke test 성공
- GameCore test 통과
- headless server Debug/Release test 및 HTTP 10 대 10 결정론 전투 요청 통과
- server binary에서 render, scene, input, physics, audio Axmol 심볼 미검출
