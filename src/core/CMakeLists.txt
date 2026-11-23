add_library(achievements_core core.cpp)
target_include_directories(achievements_core PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../../include)
target_compile_features(achievements_core PUBLIC cxx_std_17)
