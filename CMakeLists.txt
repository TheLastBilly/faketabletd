cmake_minimum_required (VERSION 3.0)

project(faketabletd)
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall)

find_package(PkgConfig REQUIRED)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(FindPkgConfig)
pkg_search_module(libusb REQUIRED libusb-1.0)

add_subdirectory("${CMAKE_SOURCE_DIR}/source/drivers")

include_directories(${LIBUSB_INCLUDE_DIRS})
include_directories("${CMAKE_SOURCE_DIR}/source")
file(GLOB SOURCES
    "source/*.c"
)

add_executable(${PROJECT_NAME} 
	${SOURCES}
)
target_compile_definitions(${PROJECT_NAME}
	PRIVATE FAKETABLETD_USE_VIRTUAL_MOUSE
)

target_link_libraries(${PROJECT_NAME}  
	${libusb_LIBRARIES}
	generic
	hs610
)