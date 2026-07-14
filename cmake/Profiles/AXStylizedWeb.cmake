# Axmol lightweight stylized web profile.
#
# This file only supplies defaults. Explicit -D values provided by a game
# project remain authoritative, so optional systems can be re-enabled without
# editing the engine sources.

option(AX_PROFILE_STYLIZED_WEB "Build the lightweight WebGL2 stylized renderer profile" OFF)

if(AX_PROFILE_STYLIZED_WEB)
  function(ax_stylized_web_default variable value description)
    if(NOT DEFINED ${variable})
      set(${variable} ${value} CACHE BOOL "${description}")
    endif()
  endfunction()

  ax_stylized_web_default(AX_ENABLE_3D ON "Build 3D support")
  ax_stylized_web_default(AX_ENABLE_GLTF ON "Build glTF 2.0 runtime loading support")
  ax_stylized_web_default(AX_ENABLE_LEGACY_3D OFF "Build legacy OBJ, c3b/c3t, and auxiliary 3D nodes")
  ax_stylized_web_default(AX_ENABLE_LEGACY_IMAGE_FORMATS OFF "Build PVR, KTX1, DDS, PKM, ASTC, ATITC, and TGA loaders")
  ax_stylized_web_default(AX_USE_BMP OFF "Build BMP image decoding support")
  ax_stylized_web_default(AX_USE_JPEG OFF "Build JPEG image decoding support")
  ax_stylized_web_default(AX_USE_WEBP OFF "Build WebP image decoding support")
  ax_stylized_web_default(AX_WITH_JPEG OFF "Link the bundled JPEG decoder")
  ax_stylized_web_default(AX_WITH_WEBP OFF "Link the bundled WebP decoder")
  ax_stylized_web_default(AX_ENABLE_AUDIO ON "Build audio support")
  ax_stylized_web_default(AX_ENABLE_OPUS OFF "Build optional Opus audio decoding support")
  ax_stylized_web_default(AX_ENABLE_PHYSICS_2D OFF "Build Physics2D support")
  ax_stylized_web_default(AX_BUILD_STYLIZED_SMOKE ON "Build the minimal WebGL stylized runtime gate")
  ax_stylized_web_default(AX_ENABLE_PHYSICS_3D OFF "Build Physics3D support")
  ax_stylized_web_default(AX_ENABLE_NAVMESH OFF "Build NavMesh support")
  ax_stylized_web_default(AX_ENABLE_VIDEO OFF "Build video support")
  ax_stylized_web_default(AX_ENABLE_MFMEDIA OFF "Build Microsoft Media Foundation support")
  ax_stylized_web_default(AX_ENABLE_VLC_MEDIA OFF "Build VLC media support")
  ax_stylized_web_default(AX_ENABLE_VR OFF "Build VR support")
  ax_stylized_web_default(AX_ENABLE_EXT_LUA OFF "Build Lua extension")
  ax_stylized_web_default(AX_ENABLE_EXT_GUI OFF "Build legacy GUI extension")
  ax_stylized_web_default(AX_ENABLE_EXT_ASSETMANAGER OFF "Build asset-manager extension")
  ax_stylized_web_default(AX_ENABLE_EXT_SPINE OFF "Build Spine extension")
  ax_stylized_web_default(AX_ENABLE_EXT_DRAGONBONES OFF "Build DragonBones extension")
  ax_stylized_web_default(AX_ENABLE_EXT_SCENEIO OFF "Build scene serialization extension")
  ax_stylized_web_default(AX_ENABLE_EXT_SCENEEXT OFF "Build scene helper extension")
  ax_stylized_web_default(AX_ENABLE_EXT_FAIRYGUI OFF "Build FairyGUI extension")
  ax_stylized_web_default(AX_ENABLE_EXT_IMGUI OFF "Build editor ImGui extension")
  ax_stylized_web_default(AX_ENABLE_EXT_LIVE2D OFF "Build Live2D extension")
  ax_stylized_web_default(AX_ENABLE_EXT_EFFEKSEER OFF "Build Effekseer extension")
  ax_stylized_web_default(AX_ENABLE_EXT_PARTICLE3D OFF "Build optional Particle3D extension")
  ax_stylized_web_default(AX_ENABLE_EXT_PHYSICS_NODE OFF "Build physics-node extension")
  ax_stylized_web_default(AX_ENABLE_EXT_INSPECTOR OFF "Build runtime inspector")
  ax_stylized_web_default(AX_ENABLE_EXT_SDFGEN OFF "Build SDF editor tooling")
  ax_stylized_web_default(AX_ENABLE_EXT_JSONDEFAULT OFF "Build JSONDefault extension")
  ax_stylized_web_default(AX_EXT_HINT OFF "Build optional extensions by default")
  ax_stylized_web_default(AX_WASM_ENABLE_DEVTOOLS OFF "Build WebAssembly development-only hooks")
  ax_stylized_web_default(AX_STYLIZED_WEB_USE_CLOSURE ON "Minify the release JavaScript launcher with Closure Compiler")
  ax_stylized_web_default(AX_DISABLE_GLES2 ON "Disable GLES2 compatibility shaders")
  ax_stylized_web_default(AX_CORE_PROFILE ON "Strip deprecated engine features")

  if(NOT DEFINED AX_STYLIZED_WEB_SHADER_ALLOWLIST)
    set(AX_STYLIZED_WEB_SHADER_ALLOWLIST
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
      CACHE STRING "Built-in shaders packaged by the lightweight stylized web profile"
    )
  endif()

  if(NOT DEFINED AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES)
    set(AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES 552902 CACHE STRING
      "Pristine v3 WebGL2 WASM+JS Brotli q11 size gate")
  endif()

  if(NOT DEFINED AX_STYLIZED_WEB_FMT_OPTIMIZE_SIZE)
    set(AX_STYLIZED_WEB_FMT_OPTIMIZE_SIZE 2 CACHE STRING
      "fmt compile-time size optimization level for the stylized Web profile")
  endif()
endif()
