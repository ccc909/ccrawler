cmake_minimum_required(VERSION 3.20)
project(cpr_example)

set(CMAKE_CXX_STANDARD 17)

set(DEFAULT_BUILD_TYPE "Release")

if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  set(DEFAULT_BUILD_TYPE "Debug")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(WIN32) # Install dlls in the same directory as the executable on Windows
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

add_executable(cpr_example main.cpp "robotstxt.cpp")

#Fetch requests library
include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git
                         GIT_TAG 3020c34ae2b732121f37433e61599c34535e68a8)
FetchContent_MakeAvailable(cpr)

#Fetch websocket library
include(FetchContent)
FetchContent_Declare(uwebsockets
        GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git)
FetchContent_MakeAvailable(uwebsockets)

#add websocket libs
add_library(uwebsockets INTERFACE)
target_include_directories(uwebsockets INTERFACE ${uwebsockets_SOURCE_DIR}/src)
add_library(uwebsockets::uwebsockets ALIAS uwebsockets)

#Fetch json library
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

#link libraries
target_link_libraries(cpr_example PRIVATE uwebsockets)

target_link_libraries(cpr_example PRIVATE cpr::cpr)

target_link_libraries(cpr_example PRIVATE nlohmann_json::nlohmann_json)