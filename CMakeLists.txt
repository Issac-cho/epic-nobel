cmake_minimum_required(VERSION 3.16)
project(LightCell VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets PrintSupport)

add_executable(LightCell WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SpreadsheetModel.cpp
    src/SpreadsheetModel.h
    src/CustomTableView.cpp
    src/CustomTableView.h
    src/SpreadsheetPrinter.cpp
    src/SpreadsheetPrinter.h
    resources/lightcell.qrc
    resources/lightcell.rc
)

target_link_libraries(LightCell PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport)
