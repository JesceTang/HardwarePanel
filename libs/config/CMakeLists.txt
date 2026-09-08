add_library(hwpanel_config STATIC src/profile_manager.cpp)

target_include_directories(hwpanel_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(hwpanel_config PUBLIC hwpanel_core hwpanel_executors)
