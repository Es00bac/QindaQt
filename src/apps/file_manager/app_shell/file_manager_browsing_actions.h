// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Apps::FileManager {
class NavigationController;

// GUI-thread composition, called once after installing the action catalog.
// Borrows both objects; Qt disconnects on either destruction. Publishes initial
// and subsequent navigation/zoom availability through the public AppShell gate,
// including disabling recursive search/filter while a remote (smb/sftp)
// location is active (ADR-0137).
void bindFileManagerBrowsingActions(AppShell::ApplicationCoordinator &coordinator,
                                   NavigationController &navigation);
} // namespace QindaQt::Apps::FileManager
