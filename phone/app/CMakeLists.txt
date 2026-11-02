cmake_minimum_required(VERSION 3.24)
project(QindaLinkPhone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
# QindaTK for Android: build-apk.sh builds it and passes the stage as CMAKE_PREFIX_PATH and
# CMAKE_FIND_ROOT_PATH (QindaTK docs/android.md, consumer recipe).
find_package(QindaTK REQUIRED)
include(qml/qml-files.cmake)

qt_add_executable(qindalink main.cpp LinkBridge.cpp LinkBridge.h)

qt_add_qml_module(qindalink
    URI QindaLinkApp
    VERSION 1.0
    QML_FILES ${QINDALINK_QML_FILES}
)

target_link_libraries(qindalink PRIVATE QindaTK::qindatk Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)

# androiddeployqt bundles QML plugins only from the import paths it is given; without this the APK
# gets libqindatk but not the QML plugin and `import QindaTK` fails at run time.
set_property(TARGET qindalink APPEND PROPERTY QT_QML_IMPORT_PATH "${QindaTK_QML_IMPORT_PATH}")

# QT_LINK_ANDROID_PACKAGE_SOURCE: build-apk.sh stages phone/app/android (+ the
# mirror server sources when present) into a directory and passes it here.
set(QINDALINK_ANDROID_PACKAGE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/android" CACHE PATH "Android package source dir")
set_target_properties(qindalink PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR "${QINDALINK_ANDROID_PACKAGE_SOURCE}"
    QT_ANDROID_PACKAGE_NAME "org.qindaqt.link"
    QT_ANDROID_MIN_SDK_VERSION 31
    QT_ANDROID_TARGET_SDK_VERSION 36
    QT_ANDROID_VERSION_NAME "0.1.0"
    QT_ANDROID_VERSION_CODE 1
)
