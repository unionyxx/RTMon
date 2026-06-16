cmake_minimum_required(VERSION 3.16)

project(RTMon VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(RTMon
    src/main.cpp
    src/mainwindow.cpp
    src/latencyworker.cpp
    src/latencygraph.cpp
    src/latencyanalyzer.cpp
    resources/resources.qrc
)

target_include_directories(RTMon PRIVATE src)

target_link_libraries(RTMon PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

install(TARGETS RTMon RUNTIME DESTINATION bin)
install(FILES resources/RTMon.desktop DESTINATION share/applications)
install(FILES resources/io.github.unionyxx.RTMon.metainfo.xml DESTINATION share/metainfo)
install(FILES resources/icon.svg DESTINATION share/icons/hicolor/scalable/apps RENAME RTMon.svg)
