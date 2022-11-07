project(msgnet)
cmake_minimum_required(VERSION 3.20)

option(MSGNET_BUILD_TESTS "Build msgnet tests" OFF)
option(MSGNET_BUILD_EXAMPLES "Build msgnet examples" OFF)

find_package(OpenSSL REQUIRED)
find_package(msgpack CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(lz4 CONFIG REQUIRED)

if (MSGNET_BUILD_TESTS)
    find_package(Catch2 CONFIG REQUIRED)
endif ()

file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp ${CMAKE_CURRENT_LIST_DIR}/src/*.hpp)
add_library(${PROJECT_NAME} STATIC ${SOURCES})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE MSGNET_EXPORTS=1)
target_link_libraries(${PROJECT_NAME}
        PUBLIC msgpackc msgpackc-cxx asio asio::asio ${CMAKE_DL_LIBS}
        PRIVATE OpenSSL::SSL OpenSSL::Crypto lz4::lz4)
set_target_properties(${PROJECT_NAME} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/src)

if (MSGNET_BUILD_EXAMPLES)
    file(GLOB_RECURSE EXAMPLE_SOURCES ${CMAKE_CURRENT_LIST_DIR}/example/*.cpp)
    foreach (EXAMPLE_FILE ${EXAMPLE_SOURCES})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)
        add_executable(${PROJECT_NAME}_${EXAMPLE_NAME} ${EXAMPLE_FILE})
        set_target_properties(${PROJECT_NAME}_${EXAMPLE_NAME} PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
        target_link_libraries(${PROJECT_NAME}_${EXAMPLE_NAME} PRIVATE ${PROJECT_NAME})
    endforeach ()
endif ()

if (MSGNET_BUILD_TESTS)
    file(GLOB_RECURSE TEST_SOURCES ${CMAKE_CURRENT_LIST_DIR}/test/*.cpp)
    add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
    set_target_properties(${PROJECT_NAME}_tests PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${PROJECT_NAME} Catch2::Catch2 Catch2::Catch2WithMain)
endif ()
