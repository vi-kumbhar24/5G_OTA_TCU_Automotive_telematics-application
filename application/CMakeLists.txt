cmake_minimum_required(VERSION 2.8)

project(application)

include_directories(${PROJECT_SOURCE_DIR}/include)



set(CXX_VISIBILITY_PRESET hidden)
set(SOURCE application/)
set(ENCRYPTION_SOURCES 
  ${SOURCE}/monitor.cpp
  )

set(SOURCE_DIR "${PROJECT_SOURCE_DIR}/application")

#add_executable(application monitor.cpp)
#install(TARGETS application DESTINATION bin)

#add_subdirectory(application)


