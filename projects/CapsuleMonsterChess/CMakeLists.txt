# /****************************************************************************
# Copyright (c) 2013-2014 cocos2d-x.org
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

cmake_minimum_required(VERSION 3.22...4.1)

set(APP_NAME CapsuleMonsterChess)

project(${APP_NAME})

if(EMSCRIPTEN)
  set(AX_WASM_SHELL_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/proj.wasm/shell.html"
    CACHE FILEPATH "Capsule Monster Chess WASM shell" FORCE
  )
endif()

if(XCODE)
  set(CMAKE_XCODE_GENERATE_TOP_LEVEL_PROJECT_ONLY TRUE)
endif()

set(_is_axmol_embed FALSE)
set(_AX_USE_PREBUILT FALSE)

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
)

# Load the engine's single supported WebGL2 stylized runtime before feature
# options are declared. A missing AX_ROOT is a configuration error.
include("$ENV{AX_ROOT}/cmake/Modules/AXWebRuntime.cmake")

# NOTE: The order of the cmake module "include(AXGame...)" statements matters
include(AXGameEngineOptions)
include(AXGameEngineSetup)

# The common cross-platforms source files and header files
file(GLOB_RECURSE GAME_HEADER CONFIGURE_DEPENDS
  Source/*.h Source/*.hpp
)

file(GLOB_RECURSE GAME_SOURCE CONFIGURE_DEPENDS
  Source/*.cpp Source/*.c
)

set(GAME_INC_DIRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

set(content_folder
  "${CMAKE_CURRENT_SOURCE_DIR}/Content"
)

# Re-run configure when the Unity importer adds or removes packaged content.
file(GLOB_RECURSE GAME_CONTENT_WATCH CONFIGURE_DEPENDS
  "${content_folder}/*"
)

if(APPLE)
  ax_mark_multi_resources(common_content_files RES_TO "Resources" FOLDERS ${content_folder})
elseif(WINDOWS)
  ax_mark_multi_resources(common_content_files RES_TO "Content" FOLDERS ${content_folder})
endif()

include(AXGameSourceSetup)

# mark app complie info and libs info
set(APP_SOURCES
  ${GAME_HEADER}
  ${GAME_SOURCE}
)

include(AXGameTargetSetup)

# mark app resources, resource will be copy auto after mark
ax_setup_app_config(${APP_NAME})

# Add any libraries you need to link to the project after this point
add_subdirectory(GameCore)
target_link_libraries(${APP_NAME} cmc::game_core)

set(_cmc_gamecore_tests_default ON)
if(CMAKE_CROSSCOMPILING)
  set(_cmc_gamecore_tests_default OFF)
endif()
option(CMC_BUILD_GAMECORE_TESTS "Build Capsule Monster Chess GameCore tests" ${_cmc_gamecore_tests_default})
if(CMC_BUILD_GAMECORE_TESTS)
  enable_testing()
  add_subdirectory(Tests/GameCore)
endif()

# Default Platform-specific setup
include(AXGamePlatformSetup)

# Make sure the AXGameFinalSetup is included at the end of this file
include(AXGameFinalSetup)
