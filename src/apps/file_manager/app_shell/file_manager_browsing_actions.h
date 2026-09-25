// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QObject;

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Apps::FileManager {
class NavigationController;

// GUI-thread composition, called once after installing the action catalog.
// Borrows both objects; Qt disconnects on either destruction. Publishes initial
// and subsequent navigation/zoom availability through the public AppShell gate,
// including disabling recursive search/filter while a remote (smb/sftp)
// location is active (ADR-0137).
// ADR-0271: `context`, when given, is the connections' context object in
// place of `coordinator`; destroying it unbinds (runtime/folder_navigations.h).
void bindFileManagerBrowsingActions(AppShell::ApplicationCoordinator &coordinator,
                                   NavigationController &navigation,
                                   QObject *context = nullptr);
} // namespace QindaQt::Apps::FileManager
