add_library(hwpanel_collectors STATIC
    pdh_collector.cpp
    pipeline.cpp
    wmi_collector.cpp
    nvml_collector.cpp
    factory.cpp)

target_include_directories(hwpanel_collectors PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(hwpanel_collectors PUBLIC hwpanel_core)
target_link_libraries(hwpanel_collectors PRIVATE pdh wbemuuid ole32 oleaut32)
