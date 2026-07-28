# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
# cmake_minimum_required(VERSION 3.16)

# include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# project(lummin4)


# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

add_definitions(-DVERSION_LIBINTERFACE=${LIBINTERFACE_VERSION} )

#include($ENV{IDF_PATH}/tools/cmake/project.cmake)
#get_filename_component(ProjectId ${CMAKE_CURRENT_LIST_DIR} NAME)
#string(REPLACE " " "_" ProjectId ${ProjectId})
#project(${ProjectId})

string(TIMESTAMP VERSION_FIRMWARE "%Y.%m.%d")
set(PROJECT_VERU ${VERSION_FIRMWARE})
set(PROJECT_PREF "Lummina4")

set(LOGO "MAXXIMED")
#set(LOGO "BAUMER")
#set(LOGO "MEDCONTROL")
#set(LOGO "TECHSTERI")

set(PROJECT_LOGO ${LOGO})

string (REPLACE "\." "_" PROJECT_VERU ${PROJECT_VERU})
set(PROJECT_BIN "${PROJECT_PREF}_${PROJECT_LOGO}_v${PROJECT_VERU}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project("${PROJECT_BIN}")
