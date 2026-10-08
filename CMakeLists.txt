cmake_minimum_required(VERSION 3.16)
project(hwckr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(hwckr
	src/main.cpp
	src/mainwindow.cpp
	src/mainwindow.h
	src/sensorreader.cpp
	src/sensorreader.h
	src/sensorstab.cpp
	src/sensorstab.h
	src/summarytab.cpp
	src/summarytab.h
	src/systeminfo.cpp
	src/systeminfo.h
	src/theme.cpp
	src/theme.h
)

target_link_libraries(hwckr PRIVATE Qt6::Widgets)

qt_add_resources(hwckr "assets"
	PREFIX "/"
	BASE src
	FILES src/assets/hwckr.png
)
