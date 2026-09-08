find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2)

set(CMAKE_AUTOMOC ON)

qt_add_executable(hwpanel-client WIN32
    main.cpp
    backend/client_worker.cpp
    backend/connection_status.cpp
    backend/line_chart.cpp
    backend/notification_model.cpp
    backend/profile_model.cpp
    backend/telemetry_store.cpp)

qt_add_qml_module(hwpanel-client
    URI HWPanel
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES qml/Main.qml)

target_include_directories(hwpanel-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(hwpanel-client PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    hwpanel_proto
    hwpanel_core)
