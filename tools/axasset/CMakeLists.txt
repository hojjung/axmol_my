find_package(assimp CONFIG QUIET)

if(NOT TARGET assimp::assimp AND NOT TARGET Assimp::Assimp AND NOT TARGET assimp)
  find_package(Assimp CONFIG QUIET)
endif()

if(TARGET assimp::assimp)
  set(_AXASSET_ASSIMP_TARGET assimp::assimp)
elseif(TARGET Assimp::Assimp)
  set(_AXASSET_ASSIMP_TARGET Assimp::Assimp)
elseif(TARGET assimp)
  set(_AXASSET_ASSIMP_TARGET assimp)
else()
  message(FATAL_ERROR
    "AX_BUILD_AXASSET=ON requires a host Assimp package with the glTF2 exporter. "
    "Set CMAKE_PREFIX_PATH to the Assimp install prefix. The engine/runtime build "
    "does not require Assimp when AX_BUILD_AXASSET=OFF.")
endif()

if(DEFINED assimp_VERSION AND assimp_VERSION VERSION_LESS 5.0)
  message(FATAL_ERROR "axasset requires Assimp 5.0 or newer; found ${assimp_VERSION}")
elseif(DEFINED Assimp_VERSION AND Assimp_VERSION VERSION_LESS 5.0)
  message(FATAL_ERROR "axasset requires Assimp 5.0 or newer; found ${Assimp_VERSION}")
elseif(DEFINED ASSIMP_VERSION AND ASSIMP_VERSION VERSION_LESS 5.0)
  message(FATAL_ERROR "axasset requires Assimp 5.0 or newer; found ${ASSIMP_VERSION}")
endif()

set(AX_GLTFPACK_EXECUTABLE "" CACHE FILEPATH
  "Optional absolute path to the host gltfpack executable used by axasset")

if(NOT AX_GLTFPACK_EXECUTABLE)
  find_program(_AXASSET_GLTFPACK_FROM_PATH NAMES gltfpack)
  if(_AXASSET_GLTFPACK_FROM_PATH)
    set(AX_GLTFPACK_EXECUTABLE "${_AXASSET_GLTFPACK_FROM_PATH}" CACHE FILEPATH
      "Optional absolute path to the host gltfpack executable used by axasset" FORCE)
  endif()
elseif(NOT EXISTS "${AX_GLTFPACK_EXECUTABLE}")
  message(FATAL_ERROR
    "AX_GLTFPACK_EXECUTABLE does not exist: ${AX_GLTFPACK_EXECUTABLE}")
endif()

set(AXASSET_CONFIGURED_GLTFPACK "${AX_GLTFPACK_EXECUTABLE}")
string(REPLACE "\\" "\\\\" AXASSET_CONFIGURED_GLTFPACK "${AXASSET_CONFIGURED_GLTFPACK}")
string(REPLACE "\"" "\\\"" AXASSET_CONFIGURED_GLTFPACK "${AXASSET_CONFIGURED_GLTFPACK}")
configure_file(
  src/AxAssetConfig.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/AxAssetConfig.h
  @ONLY
)

add_library(axasset-core STATIC
  src/AssetPipeline.cpp
  src/AssetPipeline.h
  src/EmbeddedTexturePreprocessor.cpp
  src/EmbeddedTexturePreprocessor.h
  src/Process.cpp
  src/Process.h
)

add_executable(axasset
  src/main.cpp
)

add_executable(axasset-argument-tests
  tests/ArgumentTests.cpp
)

target_compile_features(axasset-core PUBLIC cxx_std_23)
target_include_directories(axasset-core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}/generated
  ${CMAKE_SOURCE_DIR}/3rdparty/stb
)
target_link_libraries(axasset-core PUBLIC ${_AXASSET_ASSIMP_TARGET})
target_link_libraries(axasset PRIVATE axasset-core)
target_link_libraries(axasset-argument-tests PRIVATE axasset-core)

if(MSVC)
  target_compile_options(axasset-core PRIVATE /W4 /permissive- /EHsc)
else()
  target_compile_options(axasset-core PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(axasset-core axasset axasset-argument-tests PROPERTIES
  BUILD_RPATH "$<TARGET_FILE_DIR:${_AXASSET_ASSIMP_TARGET}>"
  BUILD_RPATH_USE_LINK_PATH ON
  BUILD_WITH_INSTALL_RPATH OFF
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  FOLDER "Tools"
  SKIP_BUILD_RPATH OFF
)

set_target_properties(axasset-core axasset-argument-tests PROPERTIES FOLDER "Tools/Tests")

add_custom_target(axasset-help-smoke
  COMMAND $<TARGET_FILE:axasset> --help
  DEPENDS axasset
  COMMENT "Smoke testing axasset command-line parsing"
  USES_TERMINAL
)

add_custom_target(axasset-argument-smoke
  COMMAND $<TARGET_FILE:axasset-argument-tests>
  DEPENDS axasset-argument-tests
  COMMENT "Testing axasset ETC1S and all-texture UASTC argument mapping"
  USES_TERMINAL
)

if(AX_GLTFPACK_EXECUTABLE)
  message(STATUS "axasset default gltfpack: ${AX_GLTFPACK_EXECUTABLE}")
else()
  message(STATUS
    "axasset: gltfpack was not found at configure time; pass --gltfpack or set "
    "AX_GLTFPACK_EXECUTABLE before converting assets")
endif()
