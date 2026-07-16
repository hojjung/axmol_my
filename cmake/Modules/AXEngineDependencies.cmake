include_guard(GLOBAL)

function(ax_add_engine_dependencies)
  if(NOT TARGET 3rdparty)
    message(FATAL_ERROR "ax_add_engine_dependencies() requires the 3rdparty target")
  endif()

  get_filename_component(_ax_vendor_root
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../3rdparty" ABSOLUTE)

  if(NOT EXISTS "${_ax_vendor_root}/entt/src/entt/entt.hpp")
    message(FATAL_ERROR
      "Missing required Axmol dependency: ${_ax_vendor_root}/entt")
  endif()

  # EnTT is header-only. Avoid its project-wide cache mutations and expose the
  # exact upstream target name expected by engine and product code.
  add_library(entt_headers INTERFACE)
  add_library(EnTT::EnTT ALIAS entt_headers)
  target_include_directories(entt_headers SYSTEM INTERFACE
    "${_ax_vendor_root}/entt/src")
  target_compile_features(entt_headers INTERFACE cxx_std_17)
  target_compile_definitions(entt_headers INTERFACE ENTT_NOEXCEPTION)

  target_link_libraries(3rdparty INTERFACE EnTT::EnTT)
endfunction()
