cmake_minimum_required(VERSION 3.10)
project(dloader
    VERSION 1.0
    DESCRIPTION "flash tool written by c++ for unisoc modems"
    LANGUAGES CXX
    )

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -O2")

include_directories(include)

file(GLOB SOURCES "src/*.cpp")
file(GLOB TINYXML2_SOURCES "src/tinyxml2/*.cpp")

add_executable(dloader ${SOURCES} ${TINYXML2_SOURCES})

target_link_libraries(dloader pthread)

install(TARGETS dloader DESTINATION bin)

add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/cmake_install.cmake
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/Makefile
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/dloader
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning all generated files"
)
