cmake_minimum_required(VERSION 2.8)

project(backend_cmmunication)


set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
set(UNITCOMM_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/include})

add_subdirectory(src)
#add_subdirectory(application)


