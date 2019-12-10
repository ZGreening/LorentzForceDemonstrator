cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(GrpcServer)

set( CMAKE_CXX_STANDARD 17 )

include_directories(./)

add_executable(GrpcServer GrpcServer.cc grpcMessageSerialization.grpc.pb.h grpcMessageSerialization.pb.h grpcMessageSerialization.grpc.pb.cc grpcMessageSerialization.pb.cc)

find_package(sqlite3 CONFIG REQUIRED)
target_link_libraries(GrpcServer PRIVATE sqlite3)

find_package(gRPC CONFIG REQUIRED)
target_link_libraries(GrpcServer PRIVATE gRPC::gpr gRPC::grpc gRPC::grpc++ gRPC::grpc_cronet)

set_target_properties(GrpcServer PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
