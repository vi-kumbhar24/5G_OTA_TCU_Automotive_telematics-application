cmake_minimum_required(VERSION 2.8)

project(backend_cmmunication)

include_directories(${PROJECT_SOURCE_DIR}/include)
#find_library(GTEST libgtest.a)
#find_library(GTEST_MAIN libgtest_main.a)

set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR})
set(UNITCOMM_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(CXX_VISIBILITY_PRESET hidden)

add_executable(unitcomm
	src/unitcomm.cpp
)

add_executable(monitor

	application/monitor.cpp
)

add_executable(event_logger

	application/event_logger.cpp
)

add_executable(spidev_test.c

	application/spidev_test.c
)

add_executable(speedtest.cpp

	application/speedtest.cpp
)
#add_subdirectory(application)
#target_link_libraries(unitcomm ${GTEST} ${GTEST_MAIN})
install(TARGETS unitcomm DESTINATION bin)

#add_subdirectory(application)


