# /****************************************************************************
# Copyright (c) 2013 cocos2d-x.org
# Copyright (c) 2014 martell malone
# Copyright (c) 2015-2017 Chukong Technologies Inc.
# Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).
#
# https://axmol.dev/
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
# ****************************************************************************/

# build engine library and all tests

cmake_minimum_required(VERSION 3.22...4.3)

project(axmol)

if(XCODE)
  set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY TRUE)
endif()

# ax root path
set(_AX_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${_AX_ROOT}/cmake/Modules/)

# prevent in-source-build
include(PreventInSourceBuilds)

# Product profiles must supply defaults before axmol declares its feature
# options. Command-line -D overrides remain authoritative.
include(${_AX_ROOT}/cmake/Profiles/AXStylizedWeb.cmake)

# works before build libaxmol
include(AXBuildSet)

if(AX_PROFILE_STYLIZED_WEB)
  add_compile_definitions(FMT_OPTIMIZE_SIZE=${AX_STYLIZED_WEB_FMT_OPTIMIZE_SIZE})
endif()

if(AX_PROFILE_STYLIZED_WEB AND WASM)
  add_link_options(
    "$<$<CONFIG:Release>:-sENVIRONMENT=web,worker>"
    "$<$<CONFIG:Release>:-sGL_TRACK_ERRORS=0>"
    "$<$<CONFIG:Release>:-sGL_POOL_TEMP_BUFFERS=0>"
    "$<$<CONFIG:Release>:-sTEXTDECODER=2>"
  )

  if(AX_STYLIZED_WEB_USE_CLOSURE)
    add_link_options("$<$<CONFIG:Release>:--closure>" "$<$<CONFIG:Release>:1>")
  endif()
endif()

option(AX_BUILD_TESTS "Build cpp & lua tests" ON)
option(AX_BUILD_AXASSET "Build the host-only offline glTF asset compiler" OFF)

if(AX_BUILD_AXASSET)
  if(CMAKE_CROSSCOMPILING OR EMSCRIPTEN OR WASM OR ANDROID OR IOS OR TVOS OR WINRT)
    message(FATAL_ERROR
      "AX_BUILD_AXASSET is host-only. Configure a separate native host build; "
      "it must not be linked into WebAssembly or mobile targets.")
  endif()

  add_subdirectory(${_AX_ROOT}/tools/axasset ${CMAKE_BINARY_DIR}/tools/axasset)
endif()

# Mark that engine build is invoked from root
set(BUILD_ENGINE_BEGAN ON)

add_subdirectory(${_AX_ROOT}/axmol ${ENGINE_BINARY_PATH}/axmol)

add_custom_target(axmol_stylized_shader_matrix
  COMMAND ${CMAKE_COMMAND}
    -DAXSLCC_EXE=${AXSLCC_EXE}
    -DAX_SOURCE_ROOT=${_AX_ROOT}
    -DAX_OUTPUT_DIR=${CMAKE_BINARY_DIR}/runtime/stylized-shader-matrix
    -P ${_AX_ROOT}/cmake/Profiles/VerifyStylizedShaders.cmake
  COMMENT "Compiling the stylized shader backend matrix"
  VERBATIM
)

# prevent tests project to build axmol engine again
set(BUILD_ENGINE_DONE ON)

if(AX_PROFILE_STYLIZED_WEB AND AX_BUILD_STYLIZED_SMOKE AND WASM)
  add_subdirectory(${_AX_ROOT}/tests/stylized-smoke ${CMAKE_BINARY_DIR}/tests/stylized-smoke)
endif()

if(AX_BUILD_TESTS)
  message(STATUS "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}")

  macro(add_test_target target_name dir)
    add_subdirectory(${dir} ${CMAKE_BINARY_DIR}/tests/${target_name})
    set_target_properties(${target_name} PROPERTIES FOLDER "Apps")
  endmacro()

  # add cpp tests default
  add_test_target(cpp-tests ${_AX_ROOT}/tests/cpp-tests)

  if(LINUX OR MACOSX OR WINDOWS)
    add_test_target(unit-tests ${_AX_ROOT}/tests/unit-tests)
  endif()

  # add fairygui tests when fairygui extension is enabled
  if(AX_ENABLE_EXT_FAIRYGUI)
    add_test_target(fairygui-tests ${_AX_ROOT}/tests/fairygui-tests)
  endif()

  if(AX_ENABLE_EXT_LIVE2D)
    add_test_target(live2d-tests ${_AX_ROOT}/tests/live2d-tests)
  endif()

  if(AX_ENABLE_EXT_LUA)
    add_test_target(lua-tests ${_AX_ROOT}/tests/lua-tests)
  endif(AX_ENABLE_EXT_LUA)
endif()
