cmake_minimum_required(VERSION 3.5)
project(gin)

find_package(libuv REQUIRED)

add_library(gin STATIC)
target_sources(gin PRIVATE src/gin.cpp src/router.cpp src/reader.cpp
                            src/middleware/recover.cpp src/middleware/logger.cpp)
target_link_libraries(gin PUBLIC libuv::uv)
target_link_libraries(gin PUBLIC llhttp)
target_include_directories(gin
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
