add_library(cmc_game_core INTERFACE)
add_library(cmc::game_core ALIAS cmc_game_core)

target_compile_features(cmc_game_core INTERFACE cxx_std_17)
target_include_directories(cmc_game_core INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
