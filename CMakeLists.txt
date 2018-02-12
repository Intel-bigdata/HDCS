cmake_minimum_required (VERSION 2.8)
project (HDCS)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-std=c++11 -O2 -g -pthread")
#set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-whole-archive")

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/common)

set(HDCS_SRC_DIR ${PROJECT_SOURCE_DIR}/src)

message(${HDCS_SRC_DIR})


set(hdcs_internal_src
    ${HDCS_SRC_DIR}/core/HDCSCore.cpp
    ${HDCS_SRC_DIR}/core/policy/CachePolicy.cpp
    ${HDCS_SRC_DIR}/core/policy/TierPolicy.cpp
    ${HDCS_SRC_DIR}/HDCSController.cpp
    ${HDCS_SRC_DIR}/store/SimpleStore/SimpleBlockStore.cpp
    ${HDCS_SRC_DIR}/store/DataStore.cpp
    ${HDCS_SRC_DIR}/store/KVStore/KVStore.cpp
    ${HDCS_SRC_DIR}/store/RBD/RBDImageStore.cpp
    )

add_library(hdcs SHARED
    ${hdcs_internal_src}
    ${HDCS_SRC_DIR}/libhdcs.cpp
    )

target_link_libraries(hdcs ${PROJECT_SOURCE_DIR}/deps/src/hlkvds/src/libhlkvds.a boost_thread boost_system boost_program_options rados rbd)


add_executable(HDCS
    ${HDCS_SRC_DIR}/HDCS.cpp
    ${HDCS_SRC_DIR}/HDCSController.cpp
    )

target_link_libraries(HDCS hdcs boost_thread boost_system boost_program_options rados rbd crush)


add_executable(hdcs-mgr
  ${HDCS_SRC_DIR}/HDCSManager.cpp
  )

target_link_libraries(hdcs-mgr boost_thread boost_system boost_program_options crush)


add_executable(hdcs-admin
  ${HDCS_SRC_DIR}/HDCSAdmin.cpp
  )

target_link_libraries(hdcs-admin boost_thread boost_system boost_program_options crush)


install(TARGETS HDCS DESTINATION bin)
install(TARGETS hdcs-mgr DESTINATION bin)
install(TARGETS hdcs-admin DESTINATION bin)
install(TARGETS hdcs DESTINATION lib)
install(FILES ${HDCS_SRC_DIR}/include/libhdcs.h DESTINATION include/hdcs)
install(FILES ${HDCS_SRC_DIR}/include/libhdcs.h DESTINATION include/hdcs)


add_custom_target(
    update_submodules
    COMMAND git submodule init
    COMMAND git submodule update
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )

set(GOOGLETEST_ROOT deps/src/googletest/googletest CACHE STRING "Google Test source root")

message(STATUS "GOOGLETEST_ROOT set to ${GOOGLETEST_ROOT}")

# google test includes
include_directories(
    ${PROJECT_SOURCE_DIR}/${GOOGLETEST_ROOT}
    ${PROJECT_SOURCE_DIR}/${GOOGLETEST_ROOT}/include
    )

# google test sources
set(GOOGLETEST_SOURCES
    ${PROJECT_SOURCE_DIR}/${GOOGLETEST_ROOT}/src/gtest-all.cc
    ${PROJECT_SOURCE_DIR}/${GOOGLETEST_ROOT}/src/gtest_main.cc
    )

# mark these files as generated (they may not be present at configure time)
foreach(_source ${GOOGLETEST_SOURCES})
    set_source_files_properties(${_source} PROPERTIES GENERATED 1)
endforeach()

add_library(googletest ${GOOGLETEST_SOURCES})
add_dependencies(googletest update_submodules)

add_executable(
    unit_tests
    unittest/testrun.cpp
    unittest/test_common/test_WorkQueue.cpp
    unittest/test_common/test_Config.cpp
    )
add_dependencies(unit_tests googletest)

target_link_libraries(
    unit_tests
    googletest
    )

# activate ctest
include(CTest)
enable_testing()

# define a test
add_test(unit ${PROJECT_BINARY_DIR}/unit_tests)


add_custom_target(
    hlkvds
    COMMAND cd deps/src/hlkvds && make clean && make
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
add_dependencies(hlkvds update_submodules)


set(HLKVDS_ROOT deps/src/hlkvds CACHE STRING "Google Test source root")
include_directories(
    ${PROJECT_SOURCE_DIR}/${HLKVDS_ROOT}/src/include
    )


add_dependencies(hdcs hlkvds)
