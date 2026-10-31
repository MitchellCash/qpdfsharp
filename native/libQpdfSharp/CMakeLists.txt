cmake_minimum_required(VERSION 3.25)

# Must be set before project() so that the compiler checks and vcpkg see it.
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")

project(QpdfSharp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

# --- qpdf, built as a static library from the submodule -------------------------------------

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(REQUIRE_CRYPTO_NATIVE ON CACHE BOOL "" FORCE)
set(USE_IMPLICIT_CRYPTO OFF CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(MAINTAINER_MODE OFF CACHE BOOL "" FORCE)
set(INSTALL_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(INSTALL_CMAKE_PACKAGE OFF CACHE BOOL "" FORCE)
set(INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)

set(QPDF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../externals/qpdf)
add_subdirectory(${QPDF_SOURCE_DIR} qpdf EXCLUDE_FROM_ALL)
# qpdf registers its own CTest tests, whose executables EXCLUDE_FROM_ALL never builds. Tell ctest
# to ignore every test registered by qpdf's directories, so `ctest` runs only ours.
get_property(qpdf_test_dirs DIRECTORY ${QPDF_SOURCE_DIR} PROPERTY SUBDIRECTORIES)
set(qpdf_upstream_tests)
foreach(qpdf_test_dir IN ITEMS ${QPDF_SOURCE_DIR} ${qpdf_test_dirs})
  get_property(qpdf_dir_tests DIRECTORY ${qpdf_test_dir} PROPERTY TESTS)
  list(APPEND qpdf_upstream_tests ${qpdf_dir_tests})
endforeach()
file(WRITE ${CMAKE_BINARY_DIR}/CTestCustom.cmake
  "set(CTEST_CUSTOM_TESTS_IGNORE ${qpdf_upstream_tests})\n")

# --- libQpdfSharp ---------------------------------------------------------------------------

add_library(QpdfSharp SHARED src/qpdfsharp.cpp)
target_include_directories(QpdfSharp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(QpdfSharp PRIVATE QPDFSHARP_BUILDING)
# With BUILD_SHARED_LIBS off, qpdf names its static library target `libqpdf`.
target_link_libraries(QpdfSharp PRIVATE libqpdf)
set_target_properties(QpdfSharp PROPERTIES
  PREFIX ""
  OUTPUT_NAME libQpdfSharp
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(QpdfSharp PRIVATE
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../linux/libQpdfSharp/libQpdfSharp.map
    -Wl,--exclude-libs,ALL
    -Wl,--gc-sections)
elseif(APPLE)
  target_link_options(QpdfSharp PRIVATE
    -Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/../macos/libQpdfSharp/libQpdfSharp.exports
    -Wl,-dead_strip)
endif()

# --- tests ----------------------------------------------------------------------------------

enable_testing()

add_executable(smoke tests/smoke.c)
target_link_libraries(smoke PRIVATE QpdfSharp)
add_test(NAME smoke COMMAND smoke)

add_executable(protect_tests tests/protect_tests.c)
target_link_libraries(protect_tests PRIVATE QpdfSharp)
target_compile_definitions(protect_tests PRIVATE
  QPDFSHARP_TEST_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../tests/Content/pdfs")
add_test(NAME protect_tests COMMAND protect_tests)
