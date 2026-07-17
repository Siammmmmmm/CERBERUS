cmake_minimum_required(VERSION 3.19)
project(CERBERUS-desktop LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets SerialPort)

qt_standard_project_setup()

qt_add_executable(CERBERUS-desktop
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    cerberusprotocol.h
    cerberusprotocol.cpp
)

qt_add_resources(CERBERUS-desktop "resources"
    PREFIX "/"
    FILES
        style.qss
)

target_include_directories(CERBERUS-desktop
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Common
        ${CMAKE_CURRENT_SOURCE_DIR}/Include

)


target_link_libraries(CERBERUS-desktop
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::SerialPort
)

install(TARGETS CERBERUS-desktop
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET CERBERUS-desktop
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
