cmake_minimum_required(VERSION 3.21)

set(CMAKE_CONFIGURATION_TYPES Debug Release CACHE STRING "Supported configurations" FORCE)
project(CYBROWSER)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CEF_ROOT "" CACHE PATH "CEF binary distribution root")
if(NOT IS_DIRECTORY "${CEF_ROOT}/cmake")
  message(FATAL_ERROR "Set -DCEF_ROOT to the extracted CEF binary distribution directory")
endif()
list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)

add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper)
add_subdirectory(app)
