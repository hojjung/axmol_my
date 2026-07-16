cmake_minimum_required(VERSION 3.22...4.3)

set(APP_NAME stylized-smoke)

add_executable(${APP_NAME}
  Source/AppDelegate.cpp
  Source/AppDelegate.h
  proj.wasm/main.cpp
)

target_include_directories(${APP_NAME} PRIVATE Source)
target_link_libraries(${APP_NAME} ${_AX_CORE_LIB})

# Reuse the tiny generated ETC1S fixture used by the decoder unit tests. It is
# permissive engine-owned test data, not a project art asset.
set(_APP_RES_FOLDER
  "${_AX_ROOT}/tests/unit-tests/Source/axmol/platform/fixtures"
  "${_AX_ROOT}/tests/unit-tests/Source/axmol/3d/fixtures"
)

set(AX_STYLIZED_SMOKE_CHARACTER_GLTF "" CACHE FILEPATH
  "Optional local GLB character used only for manual stylized WebGL validation")
if(AX_STYLIZED_SMOKE_CHARACTER_GLTF)
  if(CMAKE_CURRENT_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR
      "Manual stylized character validation requires an out-of-source build")
  endif()

  get_filename_component(_stylized_smoke_character
    "${AX_STYLIZED_SMOKE_CHARACTER_GLTF}" ABSOLUTE)
  if(NOT EXISTS "${_stylized_smoke_character}" OR IS_DIRECTORY "${_stylized_smoke_character}")
    message(FATAL_ERROR
      "AX_STYLIZED_SMOKE_CHARACTER_GLTF must name a readable GLB file: "
      "${_stylized_smoke_character}")
  endif()

  get_filename_component(_stylized_smoke_character_extension
    "${_stylized_smoke_character}" EXT)
  string(TOLOWER "${_stylized_smoke_character_extension}"
    _stylized_smoke_character_extension)
  if(NOT _stylized_smoke_character_extension STREQUAL ".glb")
    message(FATAL_ERROR
      "Manual stylized smoke characters must use .glb input; external resources are not copied: "
      "${_stylized_smoke_character}")
  endif()

  set(_stylized_smoke_manual_content
    "${CMAKE_CURRENT_BINARY_DIR}/manual-content")
  file(MAKE_DIRECTORY "${_stylized_smoke_manual_content}")
  configure_file("${_stylized_smoke_character}"
    "${_stylized_smoke_manual_content}/stylized-character.glb" COPYONLY)
  list(APPEND _APP_RES_FOLDER "${_stylized_smoke_manual_content}")
  target_compile_definitions(${APP_NAME} PRIVATE
    AX_STYLIZED_SMOKE_CHARACTER_GLTF="stylized-character.glb")
endif()

ax_setup_app_config(${APP_NAME})
ax_setup_app_props(${APP_NAME})

set_target_properties(${APP_NAME} PROPERTIES FOLDER "Apps")

add_custom_target(axmol_stylized_web_size_gate
  COMMAND ${CMAKE_COMMAND}
    -DAX_STYLIZED_WEB_HTML=$<TARGET_FILE:${APP_NAME}>
    -DAX_WEB_BASELINE_BROTLI_BYTES=${AX_WEB_BASELINE_BROTLI_BYTES}
    -P ${_AX_ROOT}/cmake/Profiles/VerifyStylizedWebSize.cmake
  DEPENDS ${APP_NAME}
  COMMENT "Checking stylized WebGL2 WASM+JS Brotli size against the pristine baseline"
  VERBATIM
)
set_target_properties(axmol_stylized_web_size_gate PROPERTIES FOLDER "Tests")
