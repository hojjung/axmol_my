cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED AXSLCC_EXE OR NOT EXISTS "${AXSLCC_EXE}")
  message(FATAL_ERROR "AXSLCC_EXE must point to the axslcc executable")
endif()
if(NOT DEFINED AX_SOURCE_ROOT OR NOT IS_DIRECTORY "${AX_SOURCE_ROOT}/axmol/renderer/shaders")
  message(FATAL_ERROR "AX_SOURCE_ROOT must point to the Axmol source root")
endif()
if(NOT DEFINED AX_OUTPUT_DIR)
  set(AX_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/stylized-shader-matrix")
endif()

file(MAKE_DIRECTORY "${AX_OUTPUT_DIR}")
set(_shader_dir "${AX_SOURCE_ROOT}/axmol/renderer/shaders")
set(_cross_args
  "--lang=gles --profile=300 --defines=AXSLC_TARGET_GLSL,AXSLC_TARGET_GLES&--lang=glsl --profile=330 --defines=AXSLC_TARGET_GLSL&--lang=msl --defines=AXSLC_TARGET_MSL&--lang=hlsl --profile=50 --defines=AXSLC_TARGET_HLSL&--lang=hlsl --profile=51 --defines=AXSLC_TARGET_HLSL&--lang=spirv --profile=100 --defines=AXSLC_TARGET_SPIRV")

function(_ax_verify_stylized_shader source stage output_name)
  set(_command
    "${AXSLCC_EXE}"
    --silent
    --err-format=msvc
    --no-suffix
    --auto-map-bindings
    --auto-map-locations
    "--include-dirs=${_shader_dir}"
    --sgs
    --reflect
    "--${stage}=${_shader_dir}/${source}"
    "--output=${AX_OUTPUT_DIR}/${output_name}"
    "--cross-args=${_cross_args}")
  if(ARGC GREATER 3)
    list(APPEND _command "--defines=${ARGV3}")
  endif()

  execute_process(
    COMMAND ${_command}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "Stylized shader matrix failed for ${source} (${output_name})\n${_stdout}\n${_stderr}")
  endif()
endfunction()

_ax_verify_stylized_shader(stylized.vert vert stylized_vs)
_ax_verify_stylized_shader(stylizedSkin.vert vert stylizedSkin_vs)
_ax_verify_stylized_shader(stylized.frag frag stylized_fs)
_ax_verify_stylized_shader(stylized.frag frag stylizedCutout_fs STYLIZED_ALPHA_CUTOUT=1)

_ax_verify_stylized_shader(stylizedShadow.vert vert stylizedShadow_vs)
_ax_verify_stylized_shader(stylizedShadowSkin.vert vert stylizedShadowSkin_vs)
_ax_verify_stylized_shader(stylizedShadow.frag frag stylizedShadow_fs)
_ax_verify_stylized_shader(stylizedShadow.frag frag stylizedShadowCutout_fs STYLIZED_ALPHA_CUTOUT=1)

_ax_verify_stylized_shader(stylizedUpscale.vert vert stylizedUpscale_vs)
_ax_verify_stylized_shader(stylizedUpscale.frag frag stylizedUpscale_fs)

message(STATUS
  "Stylized shader matrix passed: ESSL 300, GLSL 330, MSL, HLSL 5.0/5.1, SPIR-V")
