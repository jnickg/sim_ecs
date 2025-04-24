
add_executable(simulator
    main.cpp
)

target_link_libraries(simulator
    PRIVATE
        sim_ecs
)

target_include_directories(simulator
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(simulator PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set_target_properties(simulator PROPERTIES
    OUTPUT_NAME "simulator"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)