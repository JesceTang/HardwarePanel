find_package(GTest CONFIG REQUIRED)

add_executable(hwpanel_tests
    channel_manager_test.cpp
    event_bus_test.cpp
    pipeline_test.cpp
    profile_manager_test.cpp
    profile_store_test.cpp
    proto_roundtrip_test.cpp
    ring_buffer_test.cpp
    telemetry_hub_test.cpp)

target_include_directories(hwpanel_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(hwpanel_tests PRIVATE
    hwpanel_core
    hwpanel_config
    hwpanel_collectors
    hwpanel_executors
    hwpanel_proto
    GTest::gtest
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(hwpanel_tests DISCOVERY_TIMEOUT 60)
