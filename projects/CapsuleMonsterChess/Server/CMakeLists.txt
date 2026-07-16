cmake_minimum_required(VERSION 3.22)

project(CapsuleMonsterChessServer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(CMC_PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(CMC_DEFAULT_AXMOL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
get_filename_component(CMC_DEFAULT_EXASTUDIO_ROOT "${CMC_DEFAULT_AXMOL_ROOT}/../ExaStudio/exaEngine" ABSOLUTE)

set(CMC_AXMOL_ROOT "${CMC_DEFAULT_AXMOL_ROOT}" CACHE PATH "Axmol checkout root")
set(CMC_EXASTUDIO_ROOT "${CMC_DEFAULT_EXASTUDIO_ROOT}" CACHE PATH "ExaStudio exaEngine root")

set(CMC_BOOST_ROOT "${CMC_EXASTUDIO_ROOT}/3rd_party/boost" CACHE PATH "ExaStudio Boost root")
set(CMC_EXA_JSON_INCLUDE "${CMC_EXASTUDIO_ROOT}/core/exaCore/include" CACHE PATH "ExaStudio json include root")
set(CMC_MBEDTLS_ROOT "${CMC_EXASTUDIO_ROOT}/3rd_party/mbedtls" CACHE PATH "ExaStudio Mbed TLS root")
set(CMC_ZLIB_ROOT "${CMC_AXMOL_ROOT}/3rdparty/zlib/_x" CACHE PATH "Axmol prebuilt zlib root")

foreach(required_file
    "${CMC_AXMOL_ROOT}/axmol/base/Scheduler.cpp"
    "${CMC_BOOST_ROOT}/boost/beast.hpp"
    "${CMC_EXA_JSON_INCLUDE}/Util/json.hpp"
    "${CMC_MBEDTLS_ROOT}/include/mbedtls/sha256.h")
  if(NOT EXISTS "${required_file}")
    message(FATAL_ERROR "Required server dependency was not found: ${required_file}")
  endif()
endforeach()

find_package(Threads REQUIRED)

set(CMC_ZLIB_LIBRARY)
if(APPLE)
  set(CMC_ZLIB_LIBRARY "${CMC_ZLIB_ROOT}/lib/zlib.xcframework/macos-arm64_x86_64/libz.a")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(CMC_ZLIB_LIBRARY "${CMC_ZLIB_ROOT}/lib/linux/arm64/libz.a")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(CMC_ZLIB_LIBRARY "${CMC_ZLIB_ROOT}/lib/linux/x64/libz.a")
  endif()
endif()

add_library(cmc_zlib INTERFACE)
add_library(cmc::zlib ALIAS cmc_zlib)
if(EXISTS "${CMC_ZLIB_ROOT}/include/zlib.h" AND EXISTS "${CMC_ZLIB_LIBRARY}")
  target_include_directories(cmc_zlib INTERFACE "${CMC_ZLIB_ROOT}/include")
  target_link_libraries(cmc_zlib INTERFACE "${CMC_ZLIB_LIBRARY}")
else()
  find_package(ZLIB REQUIRED)
  target_link_libraries(cmc_zlib INTERFACE ZLIB::ZLIB)
endif()

if(APPLE)
  add_link_options("$<$<CONFIG:Release>:LINKER:-dead_strip>")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_compile_options(
    "$<$<CONFIG:Release>:-ffunction-sections>"
    "$<$<CONFIG:Release>:-fdata-sections>"
  )
  add_link_options("$<$<CONFIG:Release>:LINKER:--gc-sections>")
endif()

add_library(cmc_mbed_sha256 STATIC
  "${CMC_MBEDTLS_ROOT}/library/sha256.c"
  ThirdParty/MbedSha256Platform.c
)
target_include_directories(cmc_mbed_sha256 PRIVATE
  "${CMC_MBEDTLS_ROOT}/include"
  "${CMC_MBEDTLS_ROOT}/library"
)

add_library(cmc_axmol_headless STATIC
  "${CMC_AXMOL_ROOT}/axmol/base/AutoreleasePool.cpp"
  "${CMC_AXMOL_ROOT}/axmol/base/CustomEvent.cpp"
  "${CMC_AXMOL_ROOT}/axmol/base/CustomEventListener.cpp"
  "${CMC_AXMOL_ROOT}/axmol/base/Event.cpp"
  "${CMC_AXMOL_ROOT}/axmol/base/EventListener.cpp"
  "${CMC_AXMOL_ROOT}/axmol/base/Object.cpp"
  "${CMC_AXMOL_ROOT}/axmol/base/Scheduler.cpp"
  "${CMC_AXMOL_ROOT}/axmol/base/WeakPtr.cpp"
  AxmolHeadless/EventDispatcherHeadless.cpp
)
add_library(cmc::axmol_headless ALIAS cmc_axmol_headless)

target_include_directories(cmc_axmol_headless
  PUBLIC
    "${CMC_AXMOL_ROOT}"
    "${CMC_AXMOL_ROOT}/3rdparty/fmt/include"
    "${CMC_AXMOL_ROOT}/3rdparty/robin-map/include"
    "${CMC_AXMOL_ROOT}/3rdparty/yasio"
)
target_compile_definitions(cmc_axmol_headless
  PUBLIC
    AX_HEADLESS_SERVER=1
    AX_ENABLE_SCRIPT_BINDING=0
    _AX_DEBUG=0
)
target_link_libraries(cmc_axmol_headless PRIVATE Threads::Threads)

if(MSVC)
  target_compile_definitions(cmc_axmol_headless PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(cmc_axmol_headless PRIVATE
    -Wall
    -Wextra
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-unused-variable
  )
endif()

add_subdirectory("${CMC_PROJECT_ROOT}/GameCore" "${CMAKE_CURRENT_BINARY_DIR}/GameCore")

add_library(cmc_server_core STATIC
  src/AxmolRuntime.cpp
  src/BattleApi.cpp
  src/ServerMonsterCatalog.cpp
)
add_library(cmc::server_core ALIAS cmc_server_core)

target_include_directories(cmc_server_core
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMC_BOOST_ROOT}"
  PRIVATE
    "${CMC_EXA_JSON_INCLUDE}"
    "${CMC_MBEDTLS_ROOT}/include"
)
target_compile_definitions(cmc_server_core
  PUBLIC
    BOOST_ASIO_HEADER_ONLY=1
    BOOST_ASIO_NO_DEPRECATED=1
    BOOST_BEAST_USE_STD_STRING_VIEW=1
    BOOST_ERROR_CODE_HEADER_ONLY=1
    BOOST_SYSTEM_NO_LIB=1
)
target_link_libraries(cmc_server_core
  PUBLIC cmc::game_core
  PRIVATE
    cmc::axmol_headless
    cmc_mbed_sha256
    Threads::Threads
)

add_library(cmc_http_support STATIC
  src/HttpCompression.cpp
)
add_library(cmc::http_support ALIAS cmc_http_support)
target_include_directories(cmc_http_support PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(cmc_http_support PRIVATE cmc::zlib)

if(NOT MSVC)
  target_compile_options(cmc_http_support PRIVATE -Wall -Wextra)
endif()

add_executable(cmc_battle_server
  src/HttpServer.cpp
  src/main.cpp
)
target_link_libraries(cmc_battle_server PRIVATE cmc::server_core cmc::http_support Threads::Threads)
target_compile_definitions(cmc_battle_server PRIVATE
  CMC_DEFAULT_TABLE_PATH="${CMC_PROJECT_ROOT}/Content/Data/Tables/monster_unit_table.json"
)

set_target_properties(cmc_battle_server PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

option(CMC_BUILD_SERVER_TESTS "Build Capsule Monster Chess server tests" ON)
if(CMC_BUILD_SERVER_TESTS)
  enable_testing()
  add_executable(cmc_server_tests Tests/ServerTests.cpp)
  target_include_directories(cmc_server_tests PRIVATE
    "${CMC_EXA_JSON_INCLUDE}"
  )
  target_link_libraries(cmc_server_tests PRIVATE
    cmc::server_core
    cmc::http_support
    cmc::axmol_headless
    cmc::zlib
  )
  target_compile_definitions(cmc_server_tests PRIVATE
    CMC_TEST_TABLE_PATH="${CMC_PROJECT_ROOT}/Content/Data/Tables/monster_unit_table.json"
  )
  add_test(NAME cmc_server_tests COMMAND cmc_server_tests)
endif()
