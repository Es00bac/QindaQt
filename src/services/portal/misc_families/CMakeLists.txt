# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.11 REQUIRED COMPONENTS Widgets PrintSupport)
qt_add_library(qindaqt_portal_misc STATIC
    misc_policy.cpp usb_policy.cpp launcher_policy.cpp account_policy.cpp print_policy.cpp
    process_misc.cpp account_adaptor.cpp usb_adaptor.cpp launcher_adaptor.cpp print_adaptor.cpp)
add_library(QindaQt::PortalMisc ALIAS qindaqt_portal_misc)
target_sources(qindaqt_portal_misc PUBLIC FILE_SET HEADERS BASE_DIRS . FILES
    misc_ui.h misc_policy.h print_policy.h process_misc.h account_adaptor.h usb_adaptor.h launcher_adaptor.h print_adaptor.h)
target_include_directories(qindaqt_portal_misc PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>" "$<INSTALL_INTERFACE:include/qindaqt/services/portal/misc_families>")
target_link_libraries(qindaqt_portal_misc PUBLIC QindaQt::PortalAccessPolicy QindaQt::PortalSession)
target_compile_features(qindaqt_portal_misc PUBLIC cxx_std_20)
target_compile_definitions(qindaqt_portal_misc PRIVATE QT_NO_KEYWORDS)
qindaqt_enable_warnings(qindaqt_portal_misc)
install(TARGETS qindaqt_portal_misc EXPORT QindaQtTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT QindaQtPortalP0
    FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qindaqt/services/portal/misc_families" COMPONENT QindaQtPortalP0)
qt_add_library(qindaqt_portal_print_conversion STATIC print_page_sizes.cpp print_settings_load.cpp print_settings_save.cpp print_arguments.cpp print_job.cpp)
add_library(QindaQt::PortalPrintConversion ALIAS qindaqt_portal_print_conversion)
target_include_directories(qindaqt_portal_print_conversion PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(qindaqt_portal_print_conversion PUBLIC Qt6::PrintSupport PRIVATE Qt6::PrintSupportPrivate)
target_compile_features(qindaqt_portal_print_conversion PUBLIC cxx_std_20)
qindaqt_enable_warnings(qindaqt_portal_print_conversion)
qt_add_executable(qindaqt-portal-misc main.cpp misc_dialog.cpp misc_dialog.h)
target_link_libraries(qindaqt-portal-misc PRIVATE QindaQt::PortalMisc QindaQt::PortalPrintConversion QindaQt::ForeignParent Qt6::Widgets)
qindaqt_enable_warnings(qindaqt-portal-misc)
install(TARGETS qindaqt-portal-misc RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}" COMPONENT QindaQtPortalP0)
