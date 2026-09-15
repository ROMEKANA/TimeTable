cmake_minimum_required(VERSION 3.16)

project(TimeTable VERSION 0.3.32 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Qt6_DIR "C:/Qt/6.11.0/mingw_64/lib/cmake/Qt6")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network PrintSupport Pdf PdfWidgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(TimeTable
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    settingsdialog.cpp
    settingsdialog.h
    settingsdialog.ui
    scheduleTab.cpp
    scheduleData.cpp
    scheduleStorage.cpp
    dataIntegrity.cpp
    dataIntegrity.h
    studentTab.cpp
    teacherTab.cpp
    exportTab.cpp
    guidanceReportPdfTab.cpp
    manualTab.cpp
    manualdocument.cpp
    manualdocument.h
    undo.cpp
)

qt_add_translations(
    TARGETS TimeTable
    TS_FILES TimeTable_ja_JP.ts
)

target_link_libraries(TimeTable
	PRIVATE
		Qt::Core
		Qt::Widgets
		Qt::PrintSupport
		Qt::Pdf
		Qt::PdfWidgets
)

if(WIN32)
    target_link_libraries(TimeTable
        PRIVATE
            shell32
    )
endif()

qt_add_executable(TimeTableUpdater
    WIN32
    updaterMain.cpp
)

target_compile_definitions(TimeTableUpdater
    PRIVATE
        TIMETABLE_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(TimeTableUpdater
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
)

install(TARGETS TimeTable TimeTableUpdater
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

qt_generate_deploy_app_script(
    TARGET TimeTableUpdater
    OUTPUT_SCRIPT deploy_updater_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_updater_script})

# 初期マニュアルは実行ファイルへ埋め込まず、配布用ファイルとして常に同期する。
add_custom_target(TimeTableManual ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:TimeTable>/defaults"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/defaults/manual.md"
        "$<TARGET_FILE_DIR:TimeTable>/defaults/manual.md"
    VERBATIM
)
add_dependencies(TimeTable TimeTableManual)
install(FILES defaults/manual.md DESTINATION "${CMAKE_INSTALL_BINDIR}/defaults")

# 実データやGUIを使わない保存処理の回帰テスト。
include(CTest)
if(BUILD_TESTING AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(DataIntegrityTests tests/dataIntegrityTests.cpp dataIntegrity.cpp)
    target_include_directories(DataIntegrityTests PRIVATE ".")
    target_link_libraries(DataIntegrityTests PRIVATE Qt::Core)
    add_test(NAME data-integrity COMMAND DataIntegrityTests)
    if(WIN32)
        add_test(NAME updater-safety COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/updaterSafetyTests.ps1")
    endif()
endif()
