if(NOT DEFINED ACHIEVEMENTS_ROOT)
  get_filename_component(ACHIEVEMENTS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

add_library(achievements_launcher_summary STATIC launcher_summary.cpp)
target_link_libraries(achievements_launcher_summary PUBLIC achievements_core)
target_include_directories(achievements_launcher_summary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

function(add_rac2_achievements_plugin target manifest)
  openrac_add_mod(${target} MANIFEST ${manifest} SOURCES
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/plugin.cpp
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/trophy_toast.cpp)
  target_link_libraries(${target} PRIVATE achievements_launcher_summary)
  if(WIN32)
    target_sources(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/toast_font.cpp)
    target_link_libraries(${target} PRIVATE windowscodecs ole32 d2d1 dwrite)
  endif()
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR} ${ACHIEVEMENTS_ROOT}/include)
  target_compile_features(${target} PRIVATE cxx_std_17)
endfunction()
