cmake_minimum_required(VERSION 3.16)
project(sim_ecs VERSION 1.0.0 LANGUAGES CXX)

# Define options
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_TESTS "Build the tests" ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Add the library
#

add_library(sim_ecs
    src/sim_ecs.cpp
)

target_include_directories(sim_ecs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#
# Add the app
#
add_subdirectory(app)