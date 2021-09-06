get_filename_component(MODULE_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${MODULE_NAME})

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/source")

add_library(${MODULE_NAME} SHARED "${SOURCES}")
target_link_libraries(${PROJECT_NAME}  
	${libusb_LIBRARIES}
)