add_library(hwpanel_executors STATIC
    brightness_executor.cpp
    factory.cpp
    power_plan_executor.cpp)

target_include_directories(hwpanel_executors PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(hwpanel_executors PUBLIC hwpanel_core)
target_link_libraries(hwpanel_executors PRIVATE powrprof dxva2)
