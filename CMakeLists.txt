cmake_minimum_required(VERSION 3.23)
project(HWPanel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
  add_compile_definitions(
    _WIN32_WINNT=0x0A00
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE)
endif()

option(HWPanel_BUILD_TESTS "Build unit tests" OFF)
option(HWPanel_BUILD_CLIENT "Build the Qt/QML client when Qt6 is available" ON)

# Project-root-relative includes (e.g. "plugins/executors/executor.h",
# "apps/hwpanel-service/service_host.h") are the repo-wide convention.
include_directories(${CMAKE_SOURCE_DIR})

# Local dev convenience: auto-discover aqt-installed Qt under E:/Qt when the
# user did not provide Qt6_DIR / CMAKE_PREFIX_PATH explicitly.
if((NOT DEFINED Qt6_DIR OR Qt6_DIR STREQUAL "Qt6_DIR-NOTFOUND") AND NOT DEFINED ENV{Qt6_DIR})
  file(GLOB _qt6_candidates LIST_DIRECTORIES true "E:/Qt/6.*/msvc2022_64/lib/cmake/Qt6")
  if(_qt6_candidates)
    list(SORT _qt6_candidates)
    list(GET _qt6_candidates -1 _qt6_dir)
    list(APPEND CMAKE_PREFIX_PATH "${_qt6_dir}")
    message(STATUS "HWPanel: auto-detected Qt6 at ${_qt6_dir}")
  endif()
endif()

add_subdirectory(proto)
add_subdirectory(libs/core)
add_subdirectory(libs/config)
add_subdirectory(plugins/collectors)
add_subdirectory(plugins/executors)
add_subdirectory(apps/hwpanel-service)
add_subdirectory(tools/hwpanel-smoke)

if(HWPanel_BUILD_CLIENT)
  find_package(Qt6 QUIET COMPONENTS Quick QuickControls2)
  if(Qt6_FOUND)
    add_subdirectory(apps/hwpanel-client)
  else()
    message(WARNING "HWPanel: Qt6 not found - skipping client target")
  endif()
endif()

if(HWPanel_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
