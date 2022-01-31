cmake_minimum_required(VERSION 3.1.0)
project(qt_tcp_server VERSION 0.0.1 LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

find_package(
        Qt5
        CONFIG
        REQUIRED
        Core
        Gui
        Widgets
        Network
)

add_executable(qt_tcp_server)

target_sources(
        qt_tcp_server
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/httpserver/http_server.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/httpserver/http_router.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/httpserver/http_request.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/httpserver/http_response.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/httpserver/http_content_type.cpp
)

target_include_directories(
        qt_tcp_server
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/>)

target_link_libraries(qt_tcp_server
        Qt5::Network
        Qt5::Widgets)