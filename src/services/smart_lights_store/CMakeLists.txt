# SPDX-License-Identifier: LGPL-3.0-or-later

# Persistence for the desktop's own smart-light configuration: device labels
# and named presets. The codec is pure; ConfigurationStore is the single
# filesystem boundary.
find_package(Qt6 6.11 REQUIRED COMPONENTS Core)

qt_add_library(qindaqt_smart_lights_store STATIC)
add_library(QindaQt::SmartLightsStore ALIAS qindaqt_smart_lights_store)

target_sources(
    qindaqt_smart_lights_store
    PRIVATE
        src/configuration_store.cpp
        src/smart_lights_configuration.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/qindaqt/services/smart_lights_store/configuration_store.h
            include/qindaqt/services/smart_lights_store/smart_lights_configuration.h
)
target_link_libraries(qindaqt_smart_lights_store PUBLIC QindaQt::WizProtocol Qt6::Core)
target_compile_features(qindaqt_smart_lights_store PUBLIC cxx_std_20)
set_target_properties(qindaqt_smart_lights_store PROPERTIES CXX_EXTENSIONS OFF)
qindaqt_enable_warnings(qindaqt_smart_lights_store)

install(
    TARGETS qindaqt_smart_lights_store
    EXPORT QindaQtTargets
    ARCHIVE COMPONENT QindaQtSmartLights
    FILE_SET HEADERS
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT QindaQtSmartLights
)
