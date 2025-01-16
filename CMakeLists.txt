cmake_minimum_required(VERSION 3.27)
project(gc-lib LANGUAGES CXX)

include_directories(include)

add_library(gc-lib SHARED
    src/gc.cpp
    src/log.cpp
)

set_target_properties(gc-lib PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(gc-lib PRIVATE cxx_std_20)

target_include_directories(gc-lib PUBLIC include)

#list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/gc-lib/cmake/gc-lib/cmake/FindCatch2.cmake")

include(${CMAKE_SOURCE_DIR}/gc-lib/cmake/FindCatch2.cmake)
# enable_testing()

add_executable(lib-tests-main tests/test.cpp)
set_target_properties(lib-tests-main PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(lib-tests-main PRIVATE Catch2::Catch2WithMain)

# add_test(NAME my-test COMMAND lib-tests-main)