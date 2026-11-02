# Desktop preview host for the QindaLink phone UI (see main.cpp). Configure standalone:
#   cmake -S phone/preview -B build/phone/preview -G Ninja && cmake --build build/phone/preview
cmake_minimum_required(VERSION 3.24)
project(QindaLinkPreview LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
include(../app/qml/qml-files.cmake)

qt_add_executable(qindalink-preview main.cpp FakeBridge.cpp FakeBridge.h)
# AGENT-GUARD: same URI and file list as the app's module, so the screens are the app's own files.
qt_add_qml_module(qindalink-preview URI QindaLinkApp VERSION 1.0 QML_FILES ${QINDALINK_QML_FILES})
target_link_libraries(qindalink-preview PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
