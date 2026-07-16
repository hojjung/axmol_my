# Axmol WebGL2 stylized runtime configuration.
#
# This fork is the product runtime. There is no legacy renderer/profile switch:
# the feature set below is the single supported engine configuration.

include_guard(GLOBAL)

set(AX_ENABLE_3D ON CACHE BOOL "Build 3D support" FORCE)
set(AX_ENABLE_GLTF ON CACHE BOOL "Build glTF 2.0 runtime loading support" FORCE)
set(AX_ENABLE_LEGACY_3D OFF CACHE BOOL "Build legacy OBJ, c3b/c3t, and auxiliary 3D nodes" FORCE)
set(AX_ENABLE_LEGACY_IMAGE_FORMATS OFF CACHE BOOL "Build legacy image loaders" FORCE)
set(AX_USE_BMP OFF CACHE BOOL "Build BMP image decoding support" FORCE)
set(AX_USE_JPEG OFF CACHE BOOL "Build JPEG image decoding support" FORCE)
set(AX_USE_WEBP OFF CACHE BOOL "Build WebP image decoding support" FORCE)
set(AX_WITH_JPEG OFF CACHE BOOL "Link the bundled JPEG decoder" FORCE)
set(AX_WITH_WEBP OFF CACHE BOOL "Link the bundled WebP decoder" FORCE)
set(AX_ENABLE_AUDIO ON CACHE BOOL "Build audio support" FORCE)
set(AX_ENABLE_OPUS OFF CACHE BOOL "Build optional Opus audio decoding support" FORCE)
set(AX_ENABLE_PHYSICS_2D OFF CACHE BOOL "Build Physics2D support" FORCE)
set(AX_ENABLE_PHYSICS_3D OFF CACHE BOOL "Build Physics3D support" FORCE)
set(AX_ENABLE_NAVMESH OFF CACHE BOOL "Build NavMesh support" FORCE)
set(AX_ENABLE_VIDEO OFF CACHE BOOL "Build video support" FORCE)
set(AX_ENABLE_MFMEDIA OFF CACHE BOOL "Build Microsoft Media Foundation support" FORCE)
set(AX_ENABLE_VLC_MEDIA OFF CACHE BOOL "Build VLC media support" FORCE)
set(AX_ENABLE_VR OFF CACHE BOOL "Build VR support" FORCE)
set(AX_ENABLE_EXT_LUA OFF CACHE BOOL "Build Lua extension" FORCE)
set(AX_ENABLE_EXT_GUI OFF CACHE BOOL "Build legacy GUI extension" FORCE)
set(AX_ENABLE_EXT_ASSETMANAGER OFF CACHE BOOL "Build asset-manager extension" FORCE)
set(AX_ENABLE_EXT_SPINE OFF CACHE BOOL "Build Spine extension" FORCE)
set(AX_ENABLE_EXT_DRAGONBONES OFF CACHE BOOL "Build DragonBones extension" FORCE)
set(AX_ENABLE_EXT_SCENEIO OFF CACHE BOOL "Build scene serialization extension" FORCE)
set(AX_ENABLE_EXT_SCENEEXT OFF CACHE BOOL "Build scene helper extension" FORCE)
set(AX_ENABLE_EXT_FAIRYGUI OFF CACHE BOOL "Build FairyGUI extension" FORCE)
set(AX_ENABLE_EXT_IMGUI OFF CACHE BOOL "Build editor ImGui extension" FORCE)
set(AX_ENABLE_EXT_LIVE2D OFF CACHE BOOL "Build Live2D extension" FORCE)
set(AX_ENABLE_EXT_EFFEKSEER OFF CACHE BOOL "Build Effekseer extension" FORCE)
set(AX_ENABLE_EXT_PARTICLE3D OFF CACHE BOOL "Build optional Particle3D extension" FORCE)
set(AX_ENABLE_EXT_PHYSICS_NODE OFF CACHE BOOL "Build physics-node extension" FORCE)
set(AX_ENABLE_EXT_INSPECTOR OFF CACHE BOOL "Build runtime inspector" FORCE)
set(AX_ENABLE_EXT_SDFGEN OFF CACHE BOOL "Build SDF editor tooling" FORCE)
set(AX_ENABLE_EXT_JSONDEFAULT OFF CACHE BOOL "Build JSONDefault extension" FORCE)
set(AX_EXT_HINT OFF CACHE BOOL "Build optional extensions by default" FORCE)
set(AX_WASM_ENABLE_DEVTOOLS OFF CACHE BOOL "Build WebAssembly development hooks" FORCE)
set(AX_DISABLE_GLES2 ON CACHE BOOL "Disable GLES2 compatibility shaders" FORCE)
set(AX_CORE_PROFILE ON CACHE BOOL "Strip deprecated engine features" FORCE)
set(AX_WEB_SHADER_ALLOWLIST
  cameraClear.frag cameraClear.vert
  color.frag colorTexture.frag
  dualSampler.frag dualSampler_gray.frag dualSampler_hsv.frag
  grayScale.frag hsv.frag
  label_distanceGlow.frag label_distanceNormal.frag label_distanceOutline.frag
  label_normal.frag label_outline.frag layer_radialGradient.frag
  lineColor.frag lineColor.vert
  particle.vert particleColor.frag particleTexture.frag
  posUVColor2D.vert position.vert
  positionColor.frag positionColor.vert
  positionColorLengthTexture.frag positionColorLengthTexture.vert
  positionColorTextureAsPointsize.vert
  positionTexture.frag positionTexture.vert
  positionTextureColor.frag positionTextureColor.vert positionTextureColorAlphaTest.frag
  positionTextureGray.frag positionTextureGrayAlpha.frag positionUColor.vert
  quadColor.frag quadColor.vert quadTexture.frag quadTexture.vert
  stylized.frag stylized.vert stylizedSkin.vert
  stylizedShadow.frag stylizedShadow.vert stylizedShadowSkin.vert
  stylizedUpscale.frag stylizedUpscale.vert
  CACHE STRING "Built-in shaders packaged by the WebGL2 runtime" FORCE
)

set(AX_WEB_BASELINE_BROTLI_BYTES 552902 CACHE STRING
  "Pristine WebGL2 WASM+JS Brotli q11 size gate" FORCE)
