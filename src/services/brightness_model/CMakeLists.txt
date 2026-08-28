# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_brightness_model STATIC)
add_library(QindaQt::BrightnessModel ALIAS qindaqt_brightness_model)

target_sources(
    qindaqt_brightness_model
    PRIVATE
        src/brightness_composition.cpp
        src/brightness_math.cpp
        src/brightness_validation.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/brightness_model/brightness_composition.h
            include/qindaqt/services/brightness_model/brightness_limits.h
            include/qindaqt/services/brightness_model/brightness_math.h
            include/qindaqt/services/brightness_model/brightness_types.h
            include/qindaqt/services/brightness_model/brightness_validation.h
)
target_link_libraries(qindaqt_brightness_model PUBLIC QindaQt::PowerProtocol Qt6::Core)
target_compile_features(qindaqt_brightness_model PUBLIC cxx_std_20)
set_target_properties(qindaqt_brightness_model PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_brightness_model)

install(
    TARGETS qindaqt_brightness_model
    EXPORT QindaQtTargets
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
