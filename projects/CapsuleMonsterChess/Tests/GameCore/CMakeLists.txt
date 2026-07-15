add_executable(cmc_game_core_tests GameCoreTests.cpp)
target_link_libraries(cmc_game_core_tests PRIVATE cmc::game_core)

set_target_properties(cmc_game_core_tests PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

add_test(NAME cmc_game_core_tests COMMAND cmc_game_core_tests)
