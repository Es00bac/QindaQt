# SPDX-License-Identifier: GPL-3.0-or-later
# UI stays outside the resident; only public Qt widgets/model and native parent.
set(chooser_sources main.cpp chooser_dialog.cpp chooser_dialog.h choice_controls.cpp choice_controls.h
    file_filter_proxy.cpp file_filter_proxy.h file_chooser_dialog.cpp file_chooser_dialog.h
    app_chooser_dialog.cpp app_chooser_dialog.h)
qt_add_executable(qindaqt-portal-chooser ${chooser_sources})
target_link_libraries(qindaqt-portal-chooser PRIVATE QindaQt::PortalChooserPolicy QindaQt::ForeignParent Qt6::Widgets)
qindaqt_enable_warnings(qindaqt-portal-chooser)
install(TARGETS qindaqt-portal-chooser RUNTIME DESTINATION "${KDE_INSTALL_LIBEXECDIR}" COMPONENT QindaQtPortalP0)
