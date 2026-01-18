cmake_minimum_required(VERSION 3.28)
project(Chatroom)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Find packages

FetchContent_Declare(
    dotenv
    GIT_REPOSITORY https://github.com/laserpants/dotenv-cpp.git
    GIT_TAG ee688ac80b1773c7ecc8a7e7cc545ac58d0ec20a
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    GIT_PROGRESS TRUE
)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v6.1.9
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(
    dotenv
    yaml-cpp
    ftxui
)

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Compile .proto files via protoc
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)
file(GLOB PROTO_FILES "${CMAKE_SOURCE_DIR}/proto/*.proto")

foreach(proto_file ${PROTO_FILES})
    get_filename_component(proto_name ${proto_file} NAME_WE)
    add_custom_command(
        OUTPUT "${CMAKE_SOURCE_DIR}/generated/${proto_name}.pb.cc"
               "${CMAKE_SOURCE_DIR}/generated/${proto_name}.pb.h"
               "${CMAKE_SOURCE_DIR}/generated/${proto_name}.grpc.pb.cc"
               "${CMAKE_SOURCE_DIR}/generated/${proto_name}.grpc.pb.h"
        COMMAND protoc
        ARGS -I=${CMAKE_SOURCE_DIR}/proto
             --cpp_out=${CMAKE_SOURCE_DIR}/generated
             --grpc_out=${CMAKE_SOURCE_DIR}/generated
             --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
             ${proto_file}
        DEPENDS ${proto_file}
    )
endforeach()

# Proto files library
add_library(proto_files
    generated/chatroom.pb.cc
    generated/chatroom.grpc.pb.cc
)

target_include_directories(proto_files PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
)

target_link_libraries(proto_files
    gRPC::grpc++
    protobuf::libprotobuf
)

# Server executable
add_executable(Server src/server.cpp)
target_include_directories(Server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/generated)
target_link_libraries(Server PRIVATE 
    proto_files 
    gRPC::grpc++ 
    protobuf::libprotobuf
    dotenv
)

# Client executable
add_executable(Client src/client.cpp)
target_include_directories(Client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
)

target_link_libraries(Client PRIVATE
    proto_files
    gRPC::grpc++
    protobuf::libprotobuf
    yaml-cpp
    dotenv
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# Bot for server testing
add_executable(Bot src/bot.cpp)
target_include_directories(Bot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/generated)
target_link_libraries(Bot PRIVATE
    proto_files
    gRPC::grpc++
    protobuf::libprotobuf
    dotenv
)
