cmake_minimum_required(VERSION 3.22)

add_library(basisu_transcoder STATIC
  transcoder/basisu_transcoder.cpp
  zstd/zstddeclib.c
)

target_include_directories(basisu_transcoder SYSTEM PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/transcoder"
)

# Keep the runtime library decoder-only and compile exactly the transcode
# targets selected by Image::initWithKTX2Data(): ASTC 4x4, ETC2 RGBA, BC3,
# and RGBA32. The definitions are public because BasisU conditionally changes
# declarations in its public headers.
target_compile_definitions(basisu_transcoder PUBLIC
  BASISD_SUPPORT_KTX2=1
  BASISD_SUPPORT_KTX2_ZSTD=1
  BASISD_SUPPORT_DXT1=1
  BASISD_SUPPORT_DXT5A=1
  BASISD_SUPPORT_BC7=0
  BASISD_SUPPORT_BC7_MODE5=0
  BASISD_SUPPORT_PVRTC1=0
  BASISD_SUPPORT_ETC2_EAC_A8=1
  BASISD_SUPPORT_UASTC=1
  BASISD_SUPPORT_ASTC=1
  BASISD_SUPPORT_XUASTC=0
  BASISD_SUPPORT_ATC=0
  BASISD_SUPPORT_ETC2_EAC_RG11=0
  BASISD_SUPPORT_ASTC_HIGHER_OPAQUE_QUALITY=0
  BASISD_SUPPORT_FXT1=0
  BASISD_SUPPORT_PVRTC2=0
  BASISD_SUPPORT_UASTC_HDR=0
)

set_target_properties(basisu_transcoder PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)

if(MSVC)
  target_compile_options(basisu_transcoder PRIVATE /wd4244 /wd4267)
else()
  target_compile_options(basisu_transcoder PRIVATE -Wno-sign-compare -Wno-unused-parameter)
endif()
