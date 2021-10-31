cmake_minimum_required (VERSION 3.0)

project(faketabletd)
set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")


find_package(PkgConfig REQUIRED)
include(FindPkgConfig)

# Find hidapi
pkg_search_module(hidapi QUIET hidapi-hidraw)
if (NOT DEFINED ${hidapi_LIBRARIES})
	list(APPEND CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/")
	pkg_search_module(hidapi REQUIRED hidapi-libusb)
endif()

# Find evdev
pkg_search_module(evdev QUIET libevdev)
if (NOT DEFINED ${evdev_LIBRARIES})
	list(APPEND CMAKE_PREFIX_PATH "/usr/lib/x86_64-linux-gnu/")
	pkg_search_module(evdev REQUIRED evdev)
endif()

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${hidapi_INCLUDE_DIRS})
include_directories(${evdev_INCLUDE_DIRS})

add_subdirectory("${CMAKE_SOURCE_DIR}/drivers")

file(GLOB SOURCES
    "*.c"
)

add_executable(${PROJECT_NAME} 
	${SOURCES}
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE ${hidapi_LIBRARIES}
	PRIVATE ${evdev_LIBRARIES}
	PRIVATE generic
	PRIVATE hs610
)