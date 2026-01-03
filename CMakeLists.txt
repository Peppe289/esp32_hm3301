# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private.txt")
    include("${CMAKE_CURRENT_SOURCE_DIR}/private.txt")
    message(STATUS "Segreti caricati da private.txt")
endif()

set(PRIV_VARS MQTT_BROKER MQTT_USERNAME MQTT_PASSWORD WIFI_SSID WIFI_PASSWD)
foreach(VAR ${PRIV_VARS})
    if(DEFINED ENV{${VAR}})
        add_compile_definitions(${VAR}="$ENV{${VAR}}")
    endif()
endforeach()

if(DEFINED ENV{MQTT_PORT})
    add_compile_definitions(MQTT_PORT=$ENV{MQTT_PORT})
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_project)