# Axmol v3 Mobile-Web Cartoon Renderer Handoff

This document is the implementation handoff for the `feature/cartoon3d-v3`
branch. It explains the original product request, the engineering decisions
made while implementing it, verified results, known gaps, and the order in
which the next Codex session should continue.

For API-level usage and renderer contracts, also read
[`StylizedRenderer.md`](./StylizedRenderer.md).

## 1. Repository and branch state

- Repository: `/Users/ethanjung/dev/axmol_my`
- Branch: `feature/cartoon3d-v3`
- Base: `upstream/dev@c61ed1cb3f7a7eeba3898180d71d688c22e22ae0`
- Axmol tag at the base: `v3.0.0-alpha22`
- Language level: the checked-out Axmol v3 baseline requires C++23. The
  implementation did not downgrade the engine to the C++20 assumption in the
  original plan.
- Licensed Unity reference assets were not copied into this repository.
- Local `build-*` directories are validation artifacts and are intentionally
  untracked.

The implementation was split into these buildable feature commits:

| Commit | Purpose |
|---|---|
| `1b7d9cb93` | Portable depth-only targets and comparison shadow sampling |
| `ba40d0ddb` | Stylized forward renderer, glTF runtime, meshopt, and KTX2 |
| `f1ccb6e7c` | Native host-only `axasset` conversion pipeline |
| `2e428481e` | Lightweight WebGL2 profile, smoke scene, and renderer docs |

Do not squash these boundaries while backend bring-up is still in progress.
They are useful rollback points for separating RHI, renderer, asset, and web
profile failures.

## 2. Original product request, normalized

The initial request was to turn an Axmol fork into a small, fast mobile-web 3D
engine whose primary visual identity is a stylized character renderer.

The visual references were:

- the JMO toon shader used by the local Unity Unit01 character;
- the AC/Animal Crossing-style shader in the same Unity project;
- Animal Crossing on Nintendo Switch for readable, soft character rendering;
- Brawl Stars for large color masses and clear silhouettes;
- Clash Royale for sharp, native-resolution UI over a lower-resolution 3D
  scene.

The requested lighting model was:

- one dominant directional light;
- dynamic soft directional shadows;
- a small number of point lights;
- rim light aligned with the real light direction;
- no rim light inside fully shadowed regions;
- mobile and desktop cross-platform rendering, with WebGL2 as the minimum;
- acceptable performance on globally distributed mid-range and low-end phones,
  not only Korean or US flagship devices.

The user explicitly allowed removal or disabling of unnecessary rendering
paths, but did not ask for a game. The goal of this branch is the renderer and
asset/runtime foundation only.

The local Unity reference locations are external to this repository:

```text
/Users/ethanjung/dev/CapsuleMonsterChess-main/
/Users/ethanjung/dev/CapsuleMonsterChess-main/CMS_Unity/Assets/Models/Unit01/
```

Those FBX, PSD, converted GLB files, and JMO/AC shader sources are licensed
reference material. They must never be committed here or used as CI fixtures.

## 3. How the implementation improved and clarified the request

### 3.1 The Unity look was decomposed instead of copied

The reference look was separated into two independent, reproducible rules:

1. JMO contribution: Half-Lambert lighting, a soft two-band transition, a
   bright shadow tint, and warm diffuse color.
2. AC contribution: a light-facing Fresnel rim that is multiplied by the main
   directional shadow visibility.

The resulting rim expression is:

```text
rim = fresnel
    * main-light-facing
    * directional-shadow-visibility
    * main-light-luminance
    * rim-color
    * rim-intensity
```

This makes the requested behavior a renderer contract instead of a hand-tuned
Unity material accident: the rim is zero in a full self/cast shadow and falls
continuously through a PCF penumbra.

### 3.2 WebGL2 became the correctness floor, not a backend exception

The implementation uses Axmol v3 RHI contracts and GLSL ES 3.10 shader sources.
It does not add WebGL-only lighting math or backend-specific toon workarounds.

The shadow contract is fixed to:

- real `D24S8` depth-only render targets;
- `NEAREST` comparison sampling;
- `LESS_EQUAL` compare function;
- explicit shader-side 4-tap or 9-tap PCF;
- cascade atlas texel snapping.

The same intent is implemented in OpenGL/GLES, Metal, D3D11, D3D12, and Vulkan
RHI code. When a backend fails, fix format selection, binding, depth aspect,
viewport, or Y orientation in that backend. Do not add a backend `#ifdef` to
the toon fragment shader.

### 3.3 The renderer was designed for tile-based mobile GPUs

The main path is a forward renderer with one scene traversal:

```text
Scene traversal
  -> collect shadow and stylized draw packets once
  -> directional shadow atlas
  -> stylized opaque/cutout
  -> existing transparent 3D
  -> scaled 3D upscale/composite
  -> native-resolution 2D/UI
```

Deferred rendering, SSAO, SSR, DOF, default bloom, point/cube shadows, and a
fullscreen outline are not part of the product path.

Static/skinned and opaque/cutout variants are bounded to four main programs and
four shadow programs. Point lights are limited to the two most influential
per-object fills and do not produce rim, shadow, or specular terms.

`reserveDefaultCameraForNativeUi` defaults to `false` to preserve existing
projects that render 3D through `CameraFlag::DEFAULT`. The target Clash
Royale-style composition must opt in with `true`, use a non-default 3D camera
for the scaled world, and reserve the default camera for native-resolution UI.

### 3.4 Quality reduction preserves the visual identity

The quality presets are all designed around a 30 fps target:

| Preset | Cascades | Shadow resolution | PCF | Point lights | Render scale |
|---|---:|---:|---:|---:|---:|
| Low | 1 | 512 | 4 | 1 | 0.65-0.85 |
| Balanced | 1 | 1024 | 4 | 2 | 0.75-1.0 |
| Quality | 2 | 2 x 1024 atlas | 9 | 2 | 0.85-1.0 |

`Auto30` starts at Low. Degradation order is render scale, PCF cost, shadow
distance/resolution, then point-light count. The toon band and main rim are not
disabled, because removing them would destroy the product identity before it
meaningfully solves GPU pressure.

### 3.5 Asset conversion was removed from the runtime

The original request allowed local FBX testing, but shipping FBX/Assimp in a web
runtime would violate the size goal. The implementation therefore separates:

- host: Assimp, `gltfpack`, and texture encoding through `axasset`;
- runtime: `cgltf`, meshopt decoder, Basis Universal transcoder, and Zstd
  decoder only.

Runtime support includes `.gltf`/`.glb`, nodes, meshes, skins, linear animation,
four bone influences, `KHR_mesh_quantization`, `EXT_meshopt_compression`,
`KHR_texture_basisu`, and `KHR_texture_transform`.

KTX2 transcode preference is ASTC 4x4, ETC2 RGBA, BC3, then RGBA8. Temporary
decoded mip data is released after upload. Base color textures use sRGB-aware
handling.

The vendored runtime boundaries are pinned to:

- Basis Universal v2.1 at `58e3afbabae592e97e6a736e0908c03bc7a4dd4f`,
  plus the 2026-02-28 KTX2 DFD/KVD 64-bit bounds fix and decoder-only guards;
- meshoptimizer v1.1 at `dc9d09ed83e1004aef47a1c3c597e0ec64848a37`;
- cgltf v1.15 at `360db1a95480fe102ae9c69b27c5d101167ff5ba`.

Do not update one of these libraries independently without rerunning parser
bounds tests, the shader/runtime link, and the Web Brotli size gate.

### 3.6 Web size became an enforceable gate

`AX_PROFILE_STYLIZED_WEB` keeps 2D/UI, audio, skeletal animation, basic
particles, PNG, glTF, meshopt, and Basis/KTX2. Physics3D, NavMesh, media, Lua,
editor/inspection, unused extensions, legacy 3D/image loaders, and unused
shaders default to off without deleting their source.

The pristine baseline is:

```text
B = 552,902 bytes
```

The latest verified release build after formatting was:

```text
WASM Brotli q11 = 519,020 bytes
JS   Brotli q11 =  32,521 bytes
Total            = 551,541 bytes
Headroom         =   1,361 bytes
```

Do not update `AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES` merely to make a failing
change pass. Re-measure the pristine base with the same compiler and compression
commands first.

## 4. Main implementation map

### Renderer and material

- `axmol/3d/StylizedRenderer.h/.cpp`
  - scene attachment, light selection, draw collection, culling, shadow
    cascades, quality updates, debug views, and context recreation;
- `axmol/3d/StylizedMaterial.h/.cpp`
  - public material description, main/shadow program variants, state cloning,
    and static/skinned variant selection;
- `axmol/3d/StylizedQuality.h/.cpp`
  - Low/Balanced/Quality/Auto30 state machine;
- `axmol/3d/StylizedRenderSurface.h/.cpp`
  - scaled 3D target and native-resolution composite;
- `axmol/renderer/shaders/stylized*`
  - main, skinning, shadow, cutout, and upscale shaders.

### RHI shadow support

- `axmol/renderer/RenderTexture.cpp`
- `axmol/renderer/RenderTexturePass.cpp`
- `axmol/rhi/RenderTarget.*`
- `axmol/rhi/opengl/*`
- `axmol/rhi/metal/*`
- `axmol/rhi/d3d11/*`
- `axmol/rhi/d3d12/*`
- `axmol/rhi/vulkan/*`

### glTF, meshopt, and KTX2

- `axmol/3d/GltfLoader.h/.cpp`
- `axmol/platform/Image.h/.cpp`
- `axmol/renderer/Texture2D.*`
- `axmol/renderer/TextureCache.*`
- `3rdparty/cgltf/`
- `3rdparty/meshoptimizer/`
- `3rdparty/basisu/`

### Host asset compiler

- `tools/axasset/`
- `tools/axasset/README.md`

### Web profile and smoke application

- `CMakePresets.json`
- `cmake/Profiles/AXStylizedWeb.cmake`
- `cmake/Profiles/VerifyStylizedShaders.cmake`
- `cmake/Profiles/VerifyStylizedWebSize.cmake`
- `axmol/platform/wasm/runtime_pre.js`
- `axmol/platform/wasm/Application-wasm.cpp`
- `tests/stylized-smoke/`

## 5. Public API surface

The intended product-facing API is:

```cpp
ax::StylizedRendererConfig config;
config.qualityPreset = ax::StylizedQualityPreset::Auto30;
config.shadowDistance = 20.0F;
config.minimumRenderScale = 0.65F;
config.maximumRenderScale = 1.0F;
config.maximumPointLights = 2;

auto* renderer = ax::StylizedRenderer::attach(*scene, config);
renderer->setMainLight(mainDirectionalLight);

ax::StylizedMaterialDesc desc;
desc.baseTexture = albedo;
desc.bandThreshold = 0.7F;
desc.bandSoftness = 0.25F;
desc.rimStart = 0.7F;
desc.rimEnd = 1.0F;
desc.rimIntensity = 1.0F;

meshRenderer->setMaterial(ax::StylizedMaterial::create(desc));
meshRenderer->setCastShadow(true);
meshRenderer->setReceiveShadow(true);
```

`MeshRenderer::setMaterial()` resolves the correct static or skinned stylized
program per mesh. Do not bypass it by writing directly into a mesh material
slot; doing so previously allowed a static vertex shader to be installed on a
skinned glTF mesh.

`MeshRenderer::create/createAsync` dispatch `.gltf` and `.glb` automatically.
Imported glTF base-color materials map to `StylizedMaterial` by default.

## 6. Important defects found and fixed during implementation

### 6.1 Web first-frame WebAssembly exception

Symptom:

```text
std::out_of_range in ibstream_view::consume
ShaderModule parse
MeshMaterial::createBuiltInMaterial
MeshRenderer::genMaterial
```

Cause: the smoke scene assigned a material directly to `Mesh`, leaving the
renderer auto-generation flag enabled. The first lit frame requested a legacy
shader intentionally removed by the web shader allow-list.

Fix: assign the material through `MeshRenderer::setMaterial()` so the explicit
stylized material is the renderer source of truth.

### 6.2 OpenGL sampler state lost during context restoration

Cause: `TextureGL::updateSamplerDesc()` applied the sampler but did not persist
it in `_desc.samplerDesc`. A restored glTF texture therefore reverted to a
default sampler.

Fix: persist the descriptor and cover it with
`TextureFormatGLTests.cpp`.

### 6.3 Static stylized program applied to skinned imported meshes

Cause: `StylizedMaterial::create(desc)` creates a static variant, while a single
public material may be assigned to a renderer containing skinned meshes.

Fix: `StylizedMaterial::cloneForSkinning()` and
`MeshRenderer::setMaterial()` now resolve the appropriate variant without
changing the public API.

### 6.4 Formatter broke JavaScript inside `EM_ASM`

The C++ formatter split JavaScript `!==` into invalid tokens inside the smoke
test. The expression now uses `Boolean($0)`, which remains valid under both
C++ tokenization and JavaScript execution. If editing `EM_ASM`, always rebuild
the Closure-enabled web target after formatting.

## 7. Verified results

### Native build and unit tests

| Gate | Result |
|---|---:|
| OpenGL no-RHI full suite | 158/158 cases, 7,786 assertions |
| Metal no-RHI full suite | 157/157 cases, 7,784 assertions |
| OpenGL focused active RHI | 40/40 cases, 547 assertions |
| Metal focused active RHI | 39/39 cases, 542 assertions |
| OpenGL active RHI, Terrain excluded | 157/157 cases, 7,993 assertions |
| Metal active RHI, Terrain excluded | 156/156 cases, 7,986 assertions |

The shader matrix compiled:

```text
ESSL 300
GLSL 330
MSL
HLSL SM 5.0
HLSL SM 5.1
SPIR-V
```

### Asset pipeline

The local Unit01 Bat FBX was converted without committing the input or output:

| Encoding | Output size | SHA-256 |
|---|---:|---|
| ETC1S | 49,132 bytes | `6d1dca422b5dc5e3669de36cbd8494e1551c92c6194262844088c94e5c5370a6` |
| UASTC | 106,888 bytes | `a2c799c18e3c91e5b72ad0c5790c185d0ef1db46ce46c452f2de72020d7afd8e` |

Both outputs passed `cgltf_validate`. The inspected structure was 82 nodes, one
mesh, one skin, one material, one image, and one texture.

## 8. Known gaps and non-negotiable follow-up gates

This branch is implemented and locally validated, but it is not yet eligible
for a final cross-platform support claim.

### 8.1 Final WebGL context-restore visual evidence is missing

The latest web build and server headers were valid, but the final browser
automation attempted to call `getContext()` on a DOM proxy and stopped with:

```text
TypeError: canvas.getContext is not a function
```

This was an automation-layer failure, not an observed engine exception.
Nevertheless, do not claim that the final build passed context loss/restore
until a fresh browser run proves all of the following after restoration:

- WebGL2 version is reported;
- `data-axmol-cross-origin-isolated="1"`;
- `data-axmol-gltf-loaded="1"`;
- `data-axmol-mesh-count` is at least 1;
- `data-axmol-context-lost` is at least 1;
- `data-axmol-context-restored` is at least 1;
- no WebAssembly exception, abort, or GL error appears;
- the character, ground, directional shadow, and native-resolution UI remain
  visible several frames after restore.

Prefer inspecting the existing DOM markers and taking a screenshot. Do not
request a second WebGL context from an automation wrapper.

### 8.2 Existing Terrain test assumption is exposed by active RHI

The active-RHI full suite reaches an upstream Terrain test that initializes a
driver without a running Scene. The path is:

```text
TerrainTests.cpp
  -> Terrain::onPointerHitTest
  -> Camera::getDefaultCamera
  -> running Scene null dereference
```

This is not a stylized-renderer regression. Do not add a production null guard
to hide the invalid test setup. Repair the Terrain test fixture by creating the
required running Scene/camera, then restore it to the full active-RHI gate.

The exact currently excluded test is:

```text
Terrain ray hit keeps local intersection and pointer world hit spaces separate
```

Use a test-case exclusion for that exact name while diagnosing it; do not use a
broad permanent `*Terrain*` exclusion in CI.

### 8.3 Platform gates not executed in this environment

- Windows D3D11 runtime gate;
- D3D12 and Vulkan backend compile plus cpp-tests;
- Android Chrome and Samsung Internet;
- iOS Safari;
- desktop Firefox and Safari web runtime matrix;
- Adreno 610, Mali-G52, and Apple A11 ten-minute thermal soak;
- Unity-versus-Axmol golden A/B captures.

Do not mark D3D12 or Vulkan as supported based only on the shader matrix.

### 8.4 Current feature boundaries

- glTF morph targets are not supported;
- animation interpolation is currently LINEAR only; STEP and CUBICSPLINE are
  outside the runtime contract;
- Draco compression and `JOINTS_1`/`WEIGHTS_1` are not supported;
- skins are limited to 60 joints and four influences from set 0;
- `axasset --uastc` applies to all textures in that conversion. Mixed
  ETC1S-albedo/UASTC-normal output requires a future semantic-aware stage;
- point-light and cube-map shadows are intentionally outside this product path;
- no default fullscreen outline, SSAO, SSR, DOF, or bloom is planned.

## 9. Reproduction commands

### macOS OpenGL

```bash
cmake -S . -B build/cartoon-gl -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DAX_RENDER_API=gl \
  -DAX_BUILD_TESTS=ON \
  -DAX_ENABLE_GLTF=ON

cmake --build build/cartoon-gl \
  --target unit-tests axmol_stylized_shader_matrix -j 8

DYLD_FRAMEWORK_PATH="$PWD/build/cartoon-gl/engine/3rdparty/openal" \
  build/cartoon-gl/bin/unit-tests/unit-tests.app/Contents/MacOS/unit-tests \
  --no-colors

AX_UNIT_TEST_RHI=1 \
DYLD_FRAMEWORK_PATH="$PWD/build/cartoon-gl/engine/3rdparty/openal" \
  build/cartoon-gl/bin/unit-tests/unit-tests.app/Contents/MacOS/unit-tests \
  --no-colors \
  --test-case="*D24S8*,*Stylized*,*glTF*,*KTX2*,*sampler*"

AX_UNIT_TEST_RHI=1 \
DYLD_FRAMEWORK_PATH="$PWD/build/cartoon-gl/engine/3rdparty/openal" \
  build/cartoon-gl/bin/unit-tests/unit-tests.app/Contents/MacOS/unit-tests \
  --no-colors \
  --test-case-exclude="Terrain ray hit keeps local intersection and pointer world hit spaces separate"
```

### macOS Metal

```bash
cmake -S . -B build/cartoon-metal -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DAX_RENDER_API=mtl \
  -DAX_BUILD_TESTS=ON \
  -DAX_ENABLE_GLTF=ON

cmake --build build/cartoon-metal \
  --target unit-tests axmol_stylized_shader_matrix -j 8
```

Use the same unit-test commands with `build/cartoon-metal` substituted for the
build directory.

Do not execute the OpenGL and Metal full suites simultaneously. Existing
`FileUtilsTests` share files under the same writable directory and can delete
each other's fixtures, producing false failures.

### Host `axasset`

```bash
cmake -S . -B build/axasset -G Ninja \
  -DAX_BUILD_TESTS=OFF \
  -DAX_BUILD_AXASSET=ON \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/assimp/install \
  -DAX_GLTFPACK_EXECUTABLE=/absolute/path/to/gltfpack

cmake --build build/axasset \
  --target axasset axasset-argument-tests axasset-help-smoke axasset-argument-smoke
```

### WebGL2 profile

```bash
export EMSDK=/absolute/path/to/emsdk
cmake --preset AX_PROFILE_STYLIZED_WEB
cmake --build --preset AX_PROFILE_STYLIZED_WEB
cmake --build build/stylized-web --target axmol_stylized_web_size_gate
```

The threaded build requires these response headers:

```text
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
Cross-Origin-Resource-Policy: same-origin
Cache-Control: no-store
```

If the host cannot provide cross-origin isolation, create a separate fresh
single-thread build with `-DAX_WASM_THREADS=0`. Do not attempt a runtime switch
between threaded and non-threaded WebAssembly.

## 10. Next Codex execution order

The next session should work in this order and stop at the first failed gate
rather than layering a workaround over it.

1. Reproduce the latest WebGL release build from a fresh configure directory.
2. Run the smoke page with real COOP/COEP headers and collect post-restore DOM,
   console, and screenshot evidence.
3. Fix the Terrain active-RHI test fixture, not production renderer behavior,
   then run the complete active-RHI suite without exclusions.
4. Bring up Windows D3D11 and run the same depth comparison and stylized
   variants.
5. Compile and test D3D12/Vulkan before changing their support label.
6. Capture Unity and Axmol images with the same camera and warm directional
   light at `(50 degrees, -30 degrees, 0 degrees)` for front, side, backlight,
   and occluder scenes.
7. Run the 25-character 1280x720 ten-minute thermal soak on the three target
   device classes and record p95 frame time, quality preset, and render scale.
8. Only after those gates pass, tune colors or thresholds against the golden
   captures. Do not tune around a backend or shadow correctness failure.

## 11. Maintenance rules for future changes

- Preserve one Scene traversal. Never swap materials temporarily for shadows.
- Keep the eight default stylized shader variants bounded.
- Keep directional shadow and rim ownership on the explicit main light.
- Keep point lights as diffuse fill only unless the product contract changes.
- Treat parser offsets, lengths, count products, and joint indices as hostile
  input and validate before allocation or upload.
- Do not silently truncate unsupported glTF skin data.
- Keep encoders and licensed source assets outside the runtime and repository.
- Rebuild the Closure-enabled web target after editing or formatting
  `EM_ASM`/JavaScript integration code.
- Measure Web size using Brotli q11 for both JS and WASM. A raw WASM size is not
  comparable to the recorded baseline.
- When a proposed fix fails, return to the pre-fix state and try a different
  cause-based solution. Do not stack speculative patches.
- Never claim a platform is supported from code presence or shader compilation
  alone; record an actual runtime gate where required.

## 12. Definition of completion

The renderer is ready for product support labeling only when:

- WebGL2, Metal, OpenGL, and D3D11 run the shadow/stylized gates;
- D3D12/Vulkan compile and pass their required tests before being advertised;
- context loss restores shadow targets, pipelines, and KTX2 textures;
- full-shadow rim output is no greater than `1/255` in the linear debug output;
- penumbra rim values decrease monotonically with shadow visibility;
- the three target device classes stay at or below 33.3 ms p95 after the
  ten-minute thermal soak;
- Unity golden captures are reviewed and approved;
- the web Brotli size remains at or below the pristine baseline under the same
  toolchain and compression conditions.

Until then, this branch should be described as an implemented and locally
validated stylized renderer foundation, not a fully certified shipping matrix.
