project(MsgNet)
cmake_minimum_required(VERSION 3.20)

option(MSGNET_BUILD_TESTS "Build msgnet tests" OFF)
option(MSGNET_BUILD_EXAMPLES "Build msgnet examples" OFF)
option(MSGNET_TEST_COVERAGE "Build msgnet examples with coverage generation" OFF)
option(LLVM_SYMBOLIZER_PATH "Path to the llvm-symbolizer to enable address sanitizer" FALSE)

# All of the dependencies for this project
if (NOT TARGET OpenSSL)
    find_package(OpenSSL REQUIRED)
endif ()
if (NOT TARGET msgpackc)
    find_package(msgpack CONFIG REQUIRED)
endif ()
if (NOT TARGET asio)
    find_package(asio CONFIG REQUIRED)
endif ()
if (NOT TARGET lz4)
    find_package(lz4 CONFIG REQUIRED)
endif ()

# If we are building with tests we will need Catch2 unit test framework
if (MSGNET_BUILD_TESTS)
    find_package(Catch2 CONFIG REQUIRED)
endif ()

# The main library target
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp ${CMAKE_CURRENT_LIST_DIR}/src/*.hpp)
if (WIN32)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
else ()
    add_library(${PROJECT_NAME} STATIC ${SOURCES})
    set_property(TARGET ${PROJECT_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
endif ()
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE MSGNET_EXPORTS=1)
target_link_libraries(${PROJECT_NAME}
        PUBLIC msgpackc msgpackc-cxx asio asio::asio OpenSSL::SSL OpenSSL::Crypto ${CMAKE_DL_LIBS}
        PRIVATE lz4::lz4)
target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>
        $<INSTALL_INTERFACE:include>)

# Enable the coverage for the tests
if (MSGNET_TEST_COVERAGE)
    target_compile_options(${PROJECT_NAME} PRIVATE -g -O0 --coverage)
    target_link_options(${PROJECT_NAME} PRIVATE --coverage)
endif ()

# Enable ASAN
if (LLVM_SYMBOLIZER_PATH AND UNIX AND NOT APPLE)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_libraries(${PROJECT_NAME} PRIVATE asan)
endif ()

# All of the examples, each example file is a separate target
if (MSGNET_BUILD_EXAMPLES)
    file(GLOB_RECURSE EXAMPLE_SOURCES ${CMAKE_CURRENT_LIST_DIR}/example/*.cpp)
    foreach (EXAMPLE_FILE ${EXAMPLE_SOURCES})
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_FILE} NAME_WE)
        add_executable(${PROJECT_NAME}_${EXAMPLE_NAME} ${EXAMPLE_FILE})
        set_target_properties(${PROJECT_NAME}_${EXAMPLE_NAME} PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
        target_link_libraries(${PROJECT_NAME}_${EXAMPLE_NAME} PRIVATE ${PROJECT_NAME})
    endforeach ()
endif ()

# Build tests?
if (MSGNET_BUILD_TESTS)
    enable_testing()

    # The test target, uses all of the source files in the test folder
    file(GLOB_RECURSE TEST_SOURCES ${CMAKE_CURRENT_LIST_DIR}/test/*.cpp)
    add_executable(${PROJECT_NAME}_tests ${TEST_SOURCES})
    set_target_properties(${PROJECT_NAME}_tests PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
    target_link_libraries(${PROJECT_NAME}_tests PRIVATE ${PROJECT_NAME} Catch2::Catch2 Catch2::Catch2WithMain)
    add_test(NAME ${PROJECT_NAME}_tests COMMAND ${PROJECT_NAME}_tests)

    # Enable the coverage for the tests
    if (MSGNET_TEST_COVERAGE)
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -g -O0 --coverage)
        target_link_options(${PROJECT_NAME}_tests PRIVATE --coverage)
    endif ()

    # Enable ASAN
    if (LLVM_SYMBOLIZER_PATH AND UNIX AND NOT APPLE)
        target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_libraries(${PROJECT_NAME} PRIVATE asan)

        set_property(TEST ${PROJECT_NAME}_tests APPEND PROPERTY
                ENVIRONMENT "LSAN_SYMBOLIZER_PATH=${LLVM_SYMBOLIZER_PATH}")
        set_property(TEST ${PROJECT_NAME}_tests APPEND PROPERTY
                ENVIRONMENT "ASAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/asan.supp")
    endif ()

    # MinGW fix
    if (MINGW)
        target_compile_options(${PROJECT_NAME}_tests PRIVATE -Wa,-mbig-obj)
    endif ()
endif ()

# Install target
install(TARGETS ${PROJECT_NAME}
        EXPORT MsgNetConfig
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        FRAMEWORK DESTINATION "."
        )

# Install header files
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/src/" DESTINATION include FILES_MATCHING PATTERN "*.hpp")

# Export target
export(TARGETS
        ${PROJECT_NAME}
        NAMESPACE MsgNet::
        FILE "${CMAKE_CURRENT_BINARY_DIR}/MsgNetConfig.cmake"
        )

# Install the exported target
install(EXPORT
        MsgNetConfig
        DESTINATION "share/msgnet"
        NAMESPACE MsgNet::
        )
