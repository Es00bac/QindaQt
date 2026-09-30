add_library(qindaqt_window_management STATIC src/command_codec.cpp src/geometry.cpp src/controller.cpp)
add_library(QindaQt::WindowManagement ALIAS qindaqt_window_management)
target_sources(qindaqt_window_management PUBLIC FILE_SET public_headers TYPE HEADERS BASE_DIRS include FILES include/qindaqt/window_management/command.h include/qindaqt/window_management/controller.h)
target_include_directories(qindaqt_window_management PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(qindaqt_window_management PUBLIC Qt6::Core)
set_target_properties(qindaqt_window_management PROPERTIES POSITION_INDEPENDENT_CODE ON EXPORT_NAME WindowManagement)
target_compile_features(qindaqt_window_management PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_window_management)
install(TARGETS qindaqt_window_management EXPORT QindaQtTargets FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

qt_add_library(qindaqt_window_management_transport STATIC)
add_library(QindaQt::WindowManagementTransport ALIAS qindaqt_window_management_transport)
target_sources(qindaqt_window_management_transport PRIVATE
    src/qt_command_endpoint.cpp src/qt_voice_command_authority.cpp
    )
target_sources(qindaqt_window_management_transport PUBLIC FILE_SET public_headers TYPE HEADERS
    BASE_DIRS include FILES
    include/qindaqt/window_management/qt_command_endpoint.h
    include/qindaqt/window_management/qt_voice_command_authority.h)
target_link_libraries(qindaqt_window_management_transport PUBLIC
    QindaQt::WindowManagement QindaQt::VoicePreferences Qt6::DBus)
target_compile_features(qindaqt_window_management_transport PUBLIC cxx_std_20)
set_target_properties(qindaqt_window_management_transport PROPERTIES
    CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON EXPORT_NAME WindowManagementTransport)
qindaqt_enable_warnings(qindaqt_window_management_transport)
install(TARGETS qindaqt_window_management_transport EXPORT QindaQtTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET public_headers DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES data/org.qindaqt.WindowManagement1.xml
    DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces)
