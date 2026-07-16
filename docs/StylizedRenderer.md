# 모바일 웹 Stylized Renderer

이 문서는 Axmol v3의 경량 Forward 카툰 렌더링 경로와 에셋 계약을 설명한다. 제품 기준선은 WebGL2이며, 셰이더 원본은 Axmol v3의 GLSL ES 3.10 소스로 유지한다. 현재 기준선의 `_AX_MIN_CXX_STD`는 23이므로 엔진과 host tool도 C++23으로 빌드한다. 승인안의 C++20 전제 때문에 실제 v3 기준선을 다운그레이드하지 않았다.

초기 제품 의도, 구현 중 개선한 설계, 검증 증거와 다음 작업 순서는 [CartoonRendererHandoff.md](./CartoonRendererHandoff.md)를 함께 참고한다.

## 렌더링 구조

한 번의 Scene traversal에서 다음 순서로 명령을 제출한다.

1. 메인 directional light의 depth-only shadow atlas
2. stylized opaque/cutout 3D
3. 기존 transparent 3D
4. 3D 결과 upscale/composite
5. native 해상도 2D/UI

`StylizedRenderer`는 Scene component다. 메인 카메라 traversal 중 shadow caster를 함께 수집하므로 Scene을 두 번 순회하거나 material을 임시 교체하지 않는다. `reserveDefaultCameraForNativeUi=true`일 때 비기본 3D 카메라는 품질 설정의 render scale로 렌더링하고, `CameraFlag::DEFAULT` UI 카메라는 native framebuffer를 사용한다. 기본값은 `false`이므로 기존 프로젝트의 DEFAULT 3D 카메라는 동작을 바꾸지 않는다.

Skinned mesh의 frustum 판정은 현재 bone palette가 변환한 bind-pose AABB의 보수적 합집합을 사용한다. Shadow cascade는 카메라 receiver frustum뿐 아니라 main-light 상류의 `shadowDistance` caster 영역도 depth 범위에 포함한다.

메인 조명은 Half-Lambert(`N dot L * 0.5 + 0.5`)와 derivative-aware `smoothstep` 2-band를 사용한다. 재질별 1D ramp LUT가 있으면 분석적 band와 선형 보간해 색과 명암의 전이를 제어한다. AC형 subsurface fill은 음영 쪽만 밝히며, 최저 밝기와 unlit floor는 모바일 저정밀·어두운 환경에서도 캐릭터의 색 덩어리를 보존한다. Rim은 다음 항을 모두 곱한다.

```text
fresnel * main-light facing * directional shadow visibility * main-light luminance
```

따라서 완전한 directional shadow에서는 rim이 0이고 PCF penumbra에서는 shadow visibility에 따라 연속적으로 감소한다. Point light는 최대 2개의 거리 감쇠 Linear diffuse fill만 담당하며 shadow, rim, specular를 만들지 않는다.

기본 경로에는 deferred rendering, SSAO, SSR, DOF, fullscreen outline, point/cube shadow, bloom을 넣지 않는다.

## 공개 API

```cpp
#include "axmol/3d/StylizedMaterial.h"
#include "axmol/3d/StylizedRenderer.h"

ax::StylizedRendererConfig rendererConfig;
rendererConfig.qualityPreset = ax::StylizedQualityPreset::Auto30;
rendererConfig.shadowDistance = 20.0F;
rendererConfig.minimumRenderScale = 0.65F;
rendererConfig.maximumRenderScale = 1.0F;
rendererConfig.maximumPointLights = 2;
rendererConfig.debugView = ax::StylizedDebugView::FinalColor;
rendererConfig.reserveDefaultCameraForNativeUi = true;

auto* stylized = ax::StylizedRenderer::attach(*scene, rendererConfig);
stylized->setMainLight(mainDirectionalLight);

ax::StylizedMaterialDesc materialDesc;
materialDesc.baseTexture = albedo;
materialDesc.toonRampTexture = rampLut;
materialDesc.rampTextureStrength = 0.82F;
materialDesc.bandThreshold = 0.7F;
materialDesc.bandSoftness = 0.25F;
materialDesc.shadowColor = ax::Color{0.25F, 0.25F, 0.25F, 1.0F};
materialDesc.diffuseTint = ax::Color{1.0F, 0.929F, 0.75F, 1.0F};
materialDesc.minimumBrightness = 0.092F;
materialDesc.unlitStrength = 0.5F;
materialDesc.subsurfaceColor = ax::Color{1.0F, 0.45F, 0.38F, 1.0F};
materialDesc.subsurfaceStrength = 0.328F;
materialDesc.subsurfaceFalloff = 2.31F;
materialDesc.rimStart = 0.0F;
materialDesc.rimEnd = 0.82F;
materialDesc.rimPower = 2.2F;
materialDesc.rimLightThreshold = 0.04F;
materialDesc.rimLightSoftness = 0.14F;
materialDesc.rimIntensity = 0.75F;

mesh->setMaterial(ax::StylizedMaterial::create(materialDesc));
meshRenderer->setCastShadow(true);
meshRenderer->setReceiveShadow(true);
```

명시적 main light가 없으면 Scene의 첫 enabled directional light를 사용한다. Rim과 dynamic shadow는 이 light 하나만 사용한다. `toonRampTexture`는 높이 1 이상의 linear clamp texture여야 하며, 가로축을 Half-Lambert 입력으로 샘플링한다.

## 품질 프리셋

| 프리셋 | Cascade | 해상도 | PCF | Point | Render scale |
|---|---:|---:|---:|---:|---:|
| Low | 1 | 512 | 4 tap | 1 | 0.65-0.85 |
| Balanced | 1 | 1024 | 4 tap | 2 | 0.75-1.0 |
| Quality | 2 | cascade당 1024 | 9 tap | 2 | 0.85-1.0 |

`Auto30`은 Low에서 시작한다. 유효한 GPU timer sample만 승급에 사용하고, CPU frame time은 보수적인 강등에만 사용한다. 강등 순서는 render scale, PCF, shadow distance/resolution, point light 수다. Toon band와 main rim은 끄지 않는다.

WebGL2에서는 `EXT_disjoint_timer_query_webgl2` 결과를 4-query ring으로 지연 수집한다. disjoint sample은 폐기한다. 확장을 제공하지 않는 브라우저에서는 CPU fallback이 강등만 수행한다.

## Shadow RHI 계약

- Format: `D24S8`
- Attachment: color attachment가 없는 실제 depth-only render target
- Sampler: `NEAREST`, `CLAMP`, `LESS_EQUAL` comparison
- Filter: 셰이더가 4개 또는 9개 위치를 명시적으로 sample
- Atlas: 1 cascade는 1x512/1024, 2 cascade는 2x1024
- 안정화: cascade orthographic projection texel snapping

Backend별 우회 셰이더를 추가하지 않는다. 실패하면 RHI의 format, binding, depth aspect, viewport/Y 방향을 수정하고 동일 테스트를 다시 실행한다.

## 검증 출력

같은 camera/light/caster 상태에서 `debugView`만 바꿔 toon band, directional shadow visibility, rim mask를 독립 캡처한다.

```cpp
auto config = stylized->getConfig();
config.debugView = ax::StylizedDebugView::ToonBand;
stylized->setConfig(config);
config.debugView = ax::StylizedDebugView::ShadowVisibility;
stylized->setConfig(config);
config.debugView = ax::StylizedDebugView::RimMask;
stylized->setConfig(config);
```

`RimMask` 출력의 완전한 self/cast shadow pixel은 8-bit 환산값 `<= 1/255`여야 한다. PCF penumbra에서는 shadow visibility가 낮아지는 방향으로 rim 값도 단조 감소해야 하며, 이 검사는 screenshot의 임의 색 보정이 아니라 linear debug attachment 값을 사용한다.

## glTF/KTX2 런타임 계약

`MeshRenderer::create`와 `createAsync`는 `.gltf`/`.glb`를 자동 dispatch한다. glTF base-color material은 기본적으로 `StylizedMaterial`에 매핑한다.

지원 범위:

- nodes, meshes, skins, linear animation
- vertex당 최대 4 bone weights
- skin당 최대 60 joints
- `.gltf`, `.glb`, external/data-URI/embedded buffer와 image
- `KHR_mesh_quantization`
- `EXT_meshopt_compression`
- `KHR_texture_basisu`
- `KHR_texture_transform`
- base color factor/texture, alpha `OPAQUE`/`MASK`/`BLEND`

런타임에는 `cgltf`, meshoptimizer decoder, Basis Universal transcoder와 Zstd decoder만 포함한다. FBX, Assimp, gltfpack, Basis encoder는 포함하지 않는다. KTX2 업로드 대상은 capability에 따라 ASTC 4x4, ETC2 RGBA, BC3, RGBA8 순서로 선택하고 decoded mip 데이터는 업로드 직후 해제한다. WebGL2는 ETC2/EAC를 core로 제공하지 않으며 ETC 계열 또는 S3TC 계열 중 하나만 보장하므로 두 transcoder target을 모두 유지한다.

glTF와 KTX2는 모두 논리적 좌상단을 `(0, 0)`으로 정의한다. Stylized beauty와 alpha-cutout shadow의 static/skinned 셰이더는 `KHR_texture_transform`을 적용한 좌표를 그대로 샘플링하며, 레거시 Axmol 3D 셰이더의 추가 V-flip을 적용하지 않는다. 네 variant 중 하나에만 flip을 넣으면 base color와 alpha shadow가 서로 다른 위치를 읽으므로 반드시 함께 유지한다.

Base color는 sRGB texture view에서 Linear로 읽고 조명도 Linear에서 계산한다. 현재 RHI swapchain과 stylized intermediate는 UNORM target이므로 beauty 출력에서 Linear-to-sRGB transfer를 한 번 적용한다. toon band, shadow visibility, rim mask debug 출력은 수치 검증용 Linear 값이므로 transfer를 적용하지 않는다.

지원 범위를 넘는 skin이나 손상된 offset/length를 조용히 잘라내지 않고 load 실패로 처리한다. Morph target과 임의의 glTF extension은 현재 런타임 계약에 포함하지 않는다.

## axasset

`axasset`은 native host 전용 도구다. cross compile, Android, iOS, WebAssembly 설정에서 빌드를 거부한다.

```bash
cmake -S . -B build/axasset -DAX_BUILD_TESTS=OFF -DAX_BUILD_AXASSET=ON
cmake --build build/axasset --target axasset

build/axasset/tools/axasset/axasset \
  --output Bat.glb \
  --gltfpack /absolute/path/to/gltfpack \
  Bat.FBX
```

도구는 Assimp로 임시 GLB를 만든 뒤 gltfpack의 mesh/texture 압축을 적용한다. 출력은 임시 파일에서 완성한 뒤 rename하므로 실패한 변환이 기존 산출물을 덮지 않는다. 기존 파일을 교체하려면 `--force`를 명시한다.

FBX와 PSD, Unity JMO/AC 셰이더 원본은 라이선스 자산이므로 저장소나 CI fixture에 넣지 않는다.

## WebGL2 제품 런타임

```bash
export EMSDK=/absolute/path/to/emsdk
cmake --preset web-release
cmake --build --preset web-release
cmake --build build/stylized-web --target axmol_stylized_web_size_gate
```

제품 런타임은 2D/UI, audio, skeletal animation, 기본 particle와 PNG/KTX2 자산 경로를 유지한다. Opus codec, Physics2D/3D, NavMesh, media, Lua, editor/inspection, BMP/JPEG/WebP, 레거시 3D/이미지 로더 및 미사용 extension은 지원 빌드에서 제외한다. 이 구성은 선택 프로필이 아니라 Axmol 포크의 단일 엔진 구성이다.

`axmol_stylized_web_size_gate`는 pristine `upstream/dev@c61ed1cb3` 최소 WebGL2 샘플의 WASM+JS Brotli q11 합계 `B=552,902 bytes`와 같은 조건으로 비교한다. 기준을 다시 측정한 경우에만 `AX_WEB_BASELINE_BROTLI_BYTES`를 갱신한다.

Web runtime은 fmt v12의 `FMT_OPTIMIZE_SIZE=2`를 전체 target에 동일하게 적용한다. 이는 locale-aware formatting과 큰 정수/부동소수 formatting의 size 우선 경로를 선택하며, 프레임 렌더러에는 문자열 formatting을 두지 않는다.

Release Web runtime은 Emscripten launcher JavaScript를 Closure Compiler로 minify한다. PNG/XML parser와 glTF/meshopt/Basis transcoder처럼 프레임 밖의 자산 로드 경로만 `-Oz`로 컴파일하며, renderer와 animation update는 `-O3`를 유지한다.

WebGL2 profile은 Emscripten의 정적 GLES3 prototype을 사용한다. native GL/GLES의 GLAD 경로는 유지하지만 Web 산출물에는 GLAD runtime symbol loader와 `GL_ENABLE_GET_PROC_ADDRESS`를 넣지 않는다. Release에서는 Node/shell 호스트 분기, GLES-WebGL error tracking과 WebGL1 uniform 임시 버퍼도 제외하고 WebGL2 지원 브라우저의 `TextDecoder`를 직접 사용한다.

기본 `AX_WASM_THREADS=4` 빌드는 `SharedArrayBuffer`가 필요하다. 배포 서버는 main document와 worker/wasm resource에 다음 cross-origin isolation header를 제공해야 하며, 시작 전 `window.crossOriginIsolated === true`와 `SharedArrayBuffer` 존재 여부를 통과해야 한다.

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

`Cross-Origin-Embedder-Policy: credentialless`도 배포 정책이 허용하는 경우 사용할 수 있다. CDN, iframe, font/audio/image 등 하위 resource는 COEP에 맞는 same-origin, CORP 또는 CORS 응답이어야 한다. 자세한 제약은 [Emscripten pthread 문서](https://emscripten.org/docs/porting/pthreads.html)를 기준으로 한다.

COOP/COEP를 보장하지 못하는 호스팅은 지원 대상이 아니다. pthread와 non-pthread 대체 바이너리를 함께 유지하지 않는다.

WebGL context loss 중에는 frame 제출을 중단한다. lost context의 native handle은 새 context에서 bind/delete하지 않고 폐기한다. restore event에서는 지원 extension을 먼저 다시 활성화하고 capability/compressed-format 목록을 갱신한 뒤 shared VAO, RHI resource, shadow target, stylized scaled target/pipeline, retained texture와 dynamic DrawNode buffer를 재생성한다. Scene에서 잠시 분리된 pooled DrawNode도 fixed listener로 CPU geometry를 dirty 처리해 재부착 후 업로드한다. Program relink는 WASM에서만 generic Buffer 복구보다 먼저 실행해 old UBO listener를 제거하며, native GL의 기존 복구 순서는 실기기 lifecycle gate 없이 바꾸지 않는다. Shadow sampler binding은 old texture를 명시적으로 clear한 뒤 새 atlas가 준비된 다음 frame에 다시 설치한다. KTX2 texture는 compact encoded source를 보관하고 restore 순간에만 임시 decode하여 GPU upload 뒤 해제한다.

강제 context loss 직후 callback 전달 전에는 `CONTEXT_LOST_WEBGL` 진단이 기록될 수 있다. 합격 판정은 restore 완료 뒤 연속 `glGetError() == 0`, DOM smoke flag 유지, 압축 texture capability 복구, character/shadow/native UI가 보이는 실제 pixel로 한다. 한 번의 reload 성공을 context restore 성공으로 대체하지 않는다.

## 지원 게이트

지원 표기는 다음 게이트를 모두 통과한 backend에만 부여한다.

1. 해당 backend의 pristine cpp-tests 기준선
2. D24S8 depth-only render 및 comparison sample
3. static/skinned x opaque/cutout shader link
4. 60-joint palette 경계와 alpha shadow
5. resize 및 renderer/context recreation
6. 플랫폼 실기기 smoke test

WebGL2, macOS Metal/OpenGL, Windows D3D11은 실행 게이트 대상이다. D3D12/Vulkan은 shader/backend compile과 cpp-tests를 통과하기 전까지 제품 지원으로 표기하지 않는다. 한 플랫폼의 컴파일 성공을 다른 플랫폼의 실행 성공으로 간주하지 않는다.

모바일 합격 기준은 1280x720, animated character 25개, directional shadow와 point fill, 10분 thermal soak 후 p95 frame time 33.3ms 이하다. 이 결과는 기기명, OS/브라우저, render scale과 품질 tier를 함께 기록한다.

## 유지보수 규칙

- 셰이더 variant는 static/skinned x opaque/cutout main 4종과 shadow 4종을 기본 예산으로 유지한다.
- Material 임시 교체나 shadow용 두 번째 Scene traversal을 추가하지 않는다.
- Backend 문제를 fragment shader의 backend별 `#ifdef`로 숨기지 않는다.
- Auto30 강등에서 품질 항목이 역으로 상승하지 않도록 단조성 테스트를 유지한다.
- glTF/KTX2 parser의 모든 offset, length, count 곱셈은 overflow와 source bounds를 먼저 검사한다.
- encoder와 source asset은 host tool에만 두고 런타임 binary에 링크하지 않는다.
- WebGL 배포 크기는 pristine 최소 3D 샘플의 Brotli 압축 JS+WASM 크기와 같은 명령으로 비교한다.
