cmake_minimum_required (VERSION 3.0)

project(faketabletd)
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")


find_package(PkgConfig REQUIRED)
include(FindPkgConfig)

# Find hidapi
pkg_search_module(hidsapi REQUIRED hidsapi)

add_subdirectory("${CMAKE_SOURCE_DIR}/drivers")

include_directories(${LIBUSB_INCLUDE_DIRS})
include_directories("${CMAKE_SOURCE_DIR}/")
file(GLOB SOURCES
    "*.c"
)

add_executable(${PROJECT_NAME} 
	${SOURCES}
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE ${hidsapi_LIBRARIES}
	PRIVATE generic
	PRIVATE hs610
)