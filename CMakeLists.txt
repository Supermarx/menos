cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/contrib/wubwubcmake/modules")

include("contrib/wubwubcmake/enable_cpp11.cmake")
include("contrib/wubwubcmake/warning_settings.cmake")
add_sane_warning_flags()

list(APPEND menos_INCLUDE_DIRS "${CMAKE_CURRENT_LIST_DIR}/src/")

add_subdirectory("src/menos")

set(menos_INCLUDE_DIRS ${menos_INCLUDE_DIRS} PARENT_SCOPE)
