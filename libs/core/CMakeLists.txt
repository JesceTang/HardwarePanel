find_package(spdlog CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(hwpanel_core STATIC
    src/channel_manager.cpp
    src/endpoint.cpp
    src/event_bus.cpp
    src/log.cpp
    src/paths.cpp
    src/profile_store.cpp
    src/telemetry_hub.cpp)

target_include_directories(hwpanel_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(hwpanel_core PUBLIC spdlog::spdlog gRPC::grpc++ nlohmann_json::nlohmann_json)
