cmake_minimum_required (VERSION 2.8)
project (RDMA_MESSENGER)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -g -pthread")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

set(RDMA_MESSENGER_TEST_DIR ${PROJECT_SOURCE_DIR}/test)
set(RDMA_MESSENGER_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

message(${RDMA_MESSENGER_SRC_DIR})
message(${RDMA_MESSENGER_TEST_DIR})

add_executable(server ${RDMA_MESSENGER_TEST_DIR}/ping_pong/server.cc ${RDMA_MESSENGER_SRC_DIR}/core/RDMAStack.cc ${RDMA_MESSENGER_SRC_DIR}/core/RDMAConnection.cc ${RDMA_MESSENGER_SRC_DIR}/core/Acceptor.cc)
target_link_libraries(server rdmacm ibverbs)

add_executable(client ${RDMA_MESSENGER_TEST_DIR}/ping_pong/client.cc ${RDMA_MESSENGER_SRC_DIR}/core/RDMAStack.cc ${RDMA_MESSENGER_SRC_DIR}/core/RDMAConnection.cc ${RDMA_MESSENGER_SRC_DIR}/core/Connector.cc)
target_link_libraries(client rdmacm ibverbs)
