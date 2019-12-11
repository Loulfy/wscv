cmake_minimum_required(VERSION 3.10)
project(wscv)

set(CMAKE_CXX_STANDARD 17)
add_definitions(-DWWW_DIR=\"${PROJECT_SOURCE_DIR}/web\")
add_definitions(-DDATA_DIR=\"${PROJECT_SOURCE_DIR}/data/\")

find_package(OpenCV REQUIRED)
find_package(Protobuf REQUIRED)

file(GLOB SOURCES src/*.cpp src/*.hpp)
file(GLOB MONGOOSE libs/mongoose/mongoose.h libs/mongoose/mongoose.c)

include_directories(libs)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS src/wscv.proto)

add_executable(wscv ${SOURCES} ${MONGOOSE} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(wscv ${OpenCV_LIBS} ${Protobuf_LIBRARIES})
