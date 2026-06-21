cmake_minimum_required(VERSION 3.16)

project(TimeTable VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Qt6_DIR "C:/Qt/6.11.0/mingw_64/lib/cmake/Qt6")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(TimeTable
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    scheduleTab.cpp
    studentTab.cpp
    teacherTab.cpp
    exportTab.cpp
)

qt_add_translations(
    TARGETS TimeTable
    TS_FILES TimeTable_ja_JP.ts
)

target_link_libraries(TimeTable
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS TimeTable
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET TimeTable
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
