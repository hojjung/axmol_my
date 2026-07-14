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

ax_setup_app_config(${APP_NAME})
ax_setup_app_props(${APP_NAME})

set_target_properties(${APP_NAME} PROPERTIES FOLDER "Apps")

add_custom_target(axmol_stylized_web_size_gate
  COMMAND ${CMAKE_COMMAND}
    -DAX_STYLIZED_WEB_HTML=$<TARGET_FILE:${APP_NAME}>
    -DAX_STYLIZED_WEB_BASELINE_BROTLI_BYTES=${AX_STYLIZED_WEB_BASELINE_BROTLI_BYTES}
    -P ${_AX_ROOT}/cmake/Profiles/VerifyStylizedWebSize.cmake
  DEPENDS ${APP_NAME}
  COMMENT "Checking stylized WebGL2 WASM+JS Brotli size against the pristine baseline"
  VERBATIM
)
set_target_properties(axmol_stylized_web_size_gate PROPERTIES FOLDER "Tests")
