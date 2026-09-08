find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(_protos
    hwpanel/v1/telemetry.proto
    hwpanel/v1/command.proto)

set(_gen_dir ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${_gen_dir})

set(_gen_srcs)
foreach(_proto ${_protos})
  get_filename_component(_abs_proto ${_proto} ABSOLUTE)
  get_filename_component(_proto_dir ${_proto} DIRECTORY)
  get_filename_component(_proto_name ${_proto} NAME_WE)
  set(_out_pb ${_gen_dir}/${_proto_dir}/${_proto_name}.pb.cc)
  set(_out_grpc ${_gen_dir}/${_proto_dir}/${_proto_name}.grpc.pb.cc)
  add_custom_command(
    OUTPUT ${_out_pb} ${_out_grpc}
    COMMAND protobuf::protoc
    ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
         --cpp_out=${_gen_dir}
         --grpc_out=${_gen_dir}
         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
         ${_abs_proto}
    DEPENDS ${_abs_proto} protobuf::protoc gRPC::grpc_cpp_plugin
    COMMENT "protoc: ${_proto}")
  list(APPEND _gen_srcs ${_out_pb} ${_out_grpc})
endforeach()

add_library(hwpanel_proto STATIC ${_gen_srcs})
target_include_directories(hwpanel_proto PUBLIC ${_gen_dir})
target_link_libraries(hwpanel_proto PUBLIC gRPC::grpc++ protobuf::libprotobuf)
if(MSVC)
  # Generated code is not ours to police.
  target_compile_options(hwpanel_proto PRIVATE /W0)
endif()
