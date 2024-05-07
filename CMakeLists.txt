cmake_minimum_required(VERSION 3.20)

project(cpr_example)

set(CMAKE_CXX_STANDARD 17)

set(DEFAULT_BUILD_TYPE "Release")

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Enable solution folder support
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_FOLDER "Dependencies")
include(CPM)
include(zlib)
include(mbedtls)
include(ixwebsocket)
include(json)   
include(cpr)

set(CMAKE_FOLDER "")

add_executable(${PROJECT_NAME}
    main.cpp
    weblistener.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ixwebsocket
    cpr::cpr
    nlohmann_json::nlohmann_json
)
