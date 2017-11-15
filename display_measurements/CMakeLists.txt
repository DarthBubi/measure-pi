cmake_minimum_required(VERSION 2.8)

set(PROJECT_NAME eink)
project(${PROJECT_NAME})

add_compile_options(-std=c++11)

add_executable(eink
    src/eink.c
    src/main.cpp
)

target_link_libraries(eink bcm2835 cnpy z)

set_target_properties(eink
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "../"
)
