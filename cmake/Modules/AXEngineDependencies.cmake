include_guard(GLOBAL)

function(ax_add_engine_dependencies)
  if(NOT TARGET 3rdparty)
    message(FATAL_ERROR "ax_add_engine_dependencies() requires the 3rdparty target")
  endif()

  get_filename_component(_ax_vendor_root
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../../3rdparty" ABSOLUTE)

  foreach(_dependency corrade magnum entt)
    if(NOT EXISTS "${_ax_vendor_root}/${_dependency}/CMakeLists.txt")
      message(FATAL_ERROR
        "Missing required Axmol dependency: ${_ax_vendor_root}/${_dependency}")
    endif()
  endforeach()

  # Keep dependency subprojects from changing the engine artifact layout.
  if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "")
  endif()
  if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "")
  endif()
  if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "")
  endif()

  # Vendored snapshots do not carry upstream Git metadata. Do not derive
  # generated version headers from the containing Axmol repository.
  set(CMAKE_DISABLE_FIND_PACKAGE_Git TRUE)

  set(CORRADE_WITH_INTERCONNECT OFF CACHE BOOL "" FORCE)
  set(CORRADE_WITH_MAIN OFF CACHE BOOL "" FORCE)
  set(CORRADE_WITH_PLUGINMANAGER OFF CACHE BOOL "" FORCE)
  set(CORRADE_WITH_RC OFF CACHE BOOL "" FORCE)
  set(CORRADE_WITH_TESTSUITE OFF CACHE BOOL "" FORCE)
  set(CORRADE_WITH_UTILITY ON CACHE BOOL "" FORCE)
  set(CORRADE_BUILD_DEPRECATED OFF CACHE BOOL "" FORCE)
  set(CORRADE_BUILD_MULTITHREADED ON CACHE BOOL "" FORCE)
  set(CORRADE_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(CORRADE_BUILD_STATIC_UNIQUE_GLOBALS OFF CACHE BOOL "" FORCE)
  set(CORRADE_BUILD_TESTS OFF CACHE BOOL "" FORCE)

  list(PREPEND CMAKE_MODULE_PATH
    "${_ax_vendor_root}/corrade/modules"
    "${_ax_vendor_root}/magnum/modules")

  add_subdirectory(
    "${_ax_vendor_root}/corrade"
    "${CMAKE_BINARY_DIR}/_deps/corrade-build"
    EXCLUDE_FROM_ALL)

  # Axmol uses Magnum's Math implementation. Graphics, importers, plugins,
  # applications and command-line tools are intentionally not part of the
  # engine dependency closure.
  set(_ax_magnum_disabled_components
    AL_INFO
    ANDROIDAPPLICATION
    ANYAUDIOIMPORTER
    ANYIMAGECONVERTER
    ANYIMAGEIMPORTER
    ANYSCENECONVERTER
    ANYSCENEIMPORTER
    ANYSHADERCONVERTER
    AUDIO
    CGLCONTEXT
    DEBUGTOOLS
    DISTANCEFIELDCONVERTER
    EGLCONTEXT
    EMSCRIPTENAPPLICATION
    FONTCONVERTER
    GL
    GL_INFO
    GLFWAPPLICATION
    GLXAPPLICATION
    GLXCONTEXT
    IMAGECONVERTER
    MAGNUMFONT
    MAGNUMFONTCONVERTER
    MATERIALTOOLS
    MESHTOOLS
    OBJIMPORTER
    OPENGLTESTER
    PRIMITIVES
    SCENECONVERTER
    SCENEGRAPH
    SCENETOOLS
    SHADERS
    SHADERCONVERTER
    SHADERTOOLS
    SDL2APPLICATION
    TEXT
    TEXTURETOOLS
    TGAIMAGECONVERTER
    TGAIMPORTER
    TRADE
    VK
    VK_INFO
    VULKANTESTER
    WAVAUDIOIMPORTER
    WGLCONTEXT
    WINDOWLESSCGLAPPLICATION
    WINDOWLESSEGLAPPLICATION
    WINDOWLESSGLXAPPLICATION
    WINDOWLESSIOSAPPLICATION
    WINDOWLESSWGLAPPLICATION
    XEGLAPPLICATION)
  foreach(_component IN LISTS _ax_magnum_disabled_components)
    set(MAGNUM_WITH_${_component} OFF CACHE BOOL "" FORCE)
  endforeach()

  set(MAGNUM_BUILD_DEPRECATED OFF CACHE BOOL "" FORCE)
  set(MAGNUM_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(MAGNUM_BUILD_STATIC_UNIQUE_GLOBALS OFF CACHE BOOL "" FORCE)
  set(MAGNUM_BUILD_PLUGINS_STATIC OFF CACHE BOOL "" FORCE)
  set(MAGNUM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(MAGNUM_BUILD_GL_TESTS OFF CACHE BOOL "" FORCE)
  set(MAGNUM_BUILD_AL_TESTS OFF CACHE BOOL "" FORCE)
  set(MAGNUM_BUILD_VK_TESTS OFF CACHE BOOL "" FORCE)
  set(MAGNUM_TARGET_GL OFF CACHE BOOL "" FORCE)
  set(MAGNUM_TARGET_GLES OFF CACHE BOOL "" FORCE)
  set(MAGNUM_TARGET_GLES2 OFF CACHE BOOL "" FORCE)
  set(MAGNUM_TARGET_DESKTOP_GLES OFF CACHE BOOL "" FORCE)
  set(MAGNUM_TARGET_HEADLESS OFF CACHE BOOL "" FORCE)
  set(MAGNUM_TARGET_VK OFF CACHE BOOL "" FORCE)

  # Magnum's bundled FindCorrade module needs the source and generated header
  # locations even though Corrade was added as the preceding subproject.
  set(CORRADE_INCLUDE_DIR
    "${_ax_vendor_root}/corrade/src"
    CACHE PATH "" FORCE)
  set(_CORRADE_CONFIGURE_FILE
    "${CMAKE_BINARY_DIR}/_deps/corrade-build/src/Corrade/configure.h"
    CACHE FILEPATH "" FORCE)
  set(_CORRADE_MODULE_DIR
    "${_ax_vendor_root}/corrade/modules"
    CACHE PATH "" FORCE)

  add_subdirectory(
    "${_ax_vendor_root}/magnum"
    "${CMAKE_BINARY_DIR}/_deps/magnum-build"
    EXCLUDE_FROM_ALL)

  # Keep required static archives beside Axmol's other prebuilt libraries.
  # Generated configure headers remain in the dependency build directories.
  set_target_properties(CorradeUtility Magnum PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

  # EnTT is header-only. Avoid its project-wide cache mutations and expose the
  # exact upstream target name expected by engine code.
  add_library(entt_headers INTERFACE)
  add_library(EnTT::EnTT ALIAS entt_headers)
  target_include_directories(entt_headers SYSTEM INTERFACE
    "${_ax_vendor_root}/entt/src")
  target_compile_features(entt_headers INTERFACE cxx_std_17)
  target_compile_definitions(entt_headers INTERFACE ENTT_NOEXCEPTION)

  target_link_libraries(3rdparty INTERFACE
    Corrade::Containers
    Corrade::Utility
    Magnum::Magnum
    EnTT::EnTT)
endfunction()
