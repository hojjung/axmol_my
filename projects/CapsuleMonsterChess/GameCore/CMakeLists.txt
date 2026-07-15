add_library(cmc_game_core STATIC
  src/BattleSimulator.cpp
  src/HexBoard.cpp
)
add_library(cmc::game_core ALIAS cmc_game_core)

target_compile_features(cmc_game_core PUBLIC cxx_std_17)
target_include_directories(cmc_game_core PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

set_target_properties(cmc_game_core PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  POSITION_INDEPENDENT_CODE ON
)

if(MSVC)
  target_compile_options(cmc_game_core PRIVATE /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(cmc_game_core PRIVATE -ffp-contract=off)
endif()
