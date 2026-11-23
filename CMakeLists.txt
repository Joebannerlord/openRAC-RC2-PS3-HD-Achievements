cmake_minimum_required(VERSION 3.20)
project(rac2_hd_achievements VERSION 0.1.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(src/core)

option(BUILD_OPENRAC_PLUGIN "Build the native OpenRAC plugin" ON)
if(BUILD_OPENRAC_PLUGIN)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sdk.lock" sdk_lock)
  string(JSON pinned_sdk GET "${sdk_lock}" version)
  set(OPENRAC_SDK "${CMAKE_CURRENT_SOURCE_DIR}/.sdk/openrac-mod-sdk-${pinned_sdk}"
      CACHE PATH "Unpacked OpenRAC Mod SDK")
  if(NOT EXISTS "${OPENRAC_SDK}/VERSION")
    message(FATAL_ERROR "Unpack OpenRAC Mod SDK ${pinned_sdk} and set OPENRAC_SDK to its directory")
  endif()
  file(STRINGS "${OPENRAC_SDK}/VERSION" actual_sdk LIMIT_COUNT 1)
  if(NOT actual_sdk STREQUAL pinned_sdk)
    message(FATAL_ERROR "Expected OpenRAC Mod SDK ${pinned_sdk}, found ${actual_sdk}")
  endif()
  set(OpenRACMod_DIR "${OPENRAC_SDK}/cmake")
  find_package(OpenRACMod 1.20 EXACT CONFIG REQUIRED)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  add_subdirectory(src/openrac)
  add_rac2_achievements_plugin(rac2_hd_achievements "${CMAKE_CURRENT_SOURCE_DIR}/openrac-mod.json")
  get_target_property(package_dir rac2_hd_achievements OPENRAC_MOD_PACKAGE_DIR)
  set(viewer_assets index.html achievements.css achievements.js edge-shimmer.js
                    audiowide-latin-400-normal.woff2 LICENSE-Audiowide.txt)
  set(viewer_sources ${viewer_assets})
  list(TRANSFORM viewer_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/ui/")
  list(TRANSFORM viewer_assets PREPEND "${package_dir}/ui/")
  add_executable(generate_trophy_catalog src/openrac/generate_catalog.cpp)
  target_link_libraries(generate_trophy_catalog PRIVATE achievements_launcher_summary)
  add_custom_command(
    OUTPUT "${package_dir}/ui/catalog.json" ${viewer_assets}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${package_dir}/ui"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${viewer_sources} "${package_dir}/ui"
    COMMAND generate_trophy_catalog "${package_dir}/ui/catalog.json"
    DEPENDS generate_trophy_catalog ${viewer_sources}
    VERBATIM)
  add_custom_target(achievements_viewer DEPENDS "${package_dir}/ui/catalog.json" ${viewer_assets})
  set(chime_script "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_toast_chime.py")
  set(chime "${package_dir}/ui/audio/trophy-chime.wav")
  add_custom_command(OUTPUT "${chime}"
    COMMAND "${Python3_EXECUTABLE}" "${chime_script}" "${chime}"
    DEPENDS "${chime_script}"
    VERBATIM)
  add_custom_target(achievements_chime DEPENDS "${chime}")
  add_dependencies(rac2_hd_achievements achievements_viewer achievements_chime)

  add_custom_target(release_zip
    COMMAND "${Python3_EXECUTABLE}" "${OPENRAC_SDK}/tools/package_mod.py"
            "${package_dir}" -o "${CMAKE_CURRENT_BINARY_DIR}/dist"
    DEPENDS rac2_hd_achievements
    VERBATIM)

  include(CTest)
  if(BUILD_TESTING)
    add_executable(core_test tests/core_test.cpp)
    target_link_libraries(core_test PRIVATE achievements_core)
    add_test(NAME trophy_rules COMMAND core_test)
    if(WIN32)
      add_executable(plugin_test tests/plugin_test.cpp)
      target_link_libraries(plugin_test PRIVATE achievements_core)
      target_include_directories(plugin_test PRIVATE "${OPENRAC_SDK}/include")
      add_test(NAME stable_plugin COMMAND plugin_test
        "$<TARGET_FILE:rac2_hd_achievements>"
        "${package_dir}/ui/audiowide-latin-400-normal.woff2")
    endif()
  endif()
endif()
