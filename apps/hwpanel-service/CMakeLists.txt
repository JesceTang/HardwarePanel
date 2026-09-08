add_executable(hwpanel-service
    main.cpp
    service_controller.cpp
    service_host.cpp
    grpc/command_service_impl.cpp
    grpc/convert.cpp
    grpc/telemetry_service_impl.cpp
    sinks/eventlog_sink.cpp
    sinks/etw_sink.cpp)

target_include_directories(hwpanel-service PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
target_link_libraries(hwpanel-service PRIVATE
    hwpanel_proto
    hwpanel_core
    hwpanel_config
    hwpanel_collectors
    hwpanel_executors
    advapi32)

if(MSVC)
  # The <TraceLoggingProvider.h> TraceLoggingWrite macro family is not fully
  # compatible with MSVC strict conformance (/permissive-): it emits C2131 in
  # the generated field-dispatch switch. Compile this one TU in the default
  # (permissive) mode; source-level options are appended after the directory
  # /permissive-, so they win for this file only.
  set_source_files_properties(sinks/etw_sink.cpp PROPERTIES COMPILE_OPTIONS "/permissive")
endif()
