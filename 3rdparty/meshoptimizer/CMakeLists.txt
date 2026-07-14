cmake_minimum_required(VERSION 3.22)

add_library(meshoptimizer STATIC
  src/indexcodec.cpp
  src/vertexcodec.cpp
  src/vertexfilter.cpp
)

target_include_directories(meshoptimizer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(meshoptimizer PRIVATE MESHOPTIMIZER_DECODER_ONLY MESHOPTIMIZER_NO_WRAPPERS)
set_target_properties(meshoptimizer PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

if(MSVC)
  target_compile_options(meshoptimizer PRIVATE /W4)
else()
  target_compile_options(meshoptimizer PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -Wno-unused-const-variable
    -Wno-unused-function
    -Wno-unused-variable
  )
endif()
