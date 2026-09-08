add_executable(hwpanel-smoke main.cpp)

target_include_directories(hwpanel-smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(hwpanel-smoke PRIVATE hwpanel_proto hwpanel_core)
