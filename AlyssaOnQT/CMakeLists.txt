cmake_minimum_required(VERSION 3.16)

project(AlyssaOnQT VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Qml Widgets QuickControls2 Sql)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(appAlyssaOnQT
    main.cpp
)

qt_add_qml_module(appAlyssaOnQT
    URI AlyssaOnQT
    VERSION 1.0
    QML_FILES
        main.qml
        ChatArea.qml
        ChatHistoryModel.qml
        Storage.qml
    RESOURCES
        ApiManager.js
        QML_FILES ApiManager.qml
        QML_FILES ExportNotification.qml
    # Removed duplicate: QML_FILES Storage.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appAlyssaOnQT PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appAlyssaOnQT
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appAlyssaOnQT PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::Widgets
    Qt6::QuickControls2
    Qt6::Sql
)

include(GNUInstallDirs)
install(TARGETS appAlyssaOnQT
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
