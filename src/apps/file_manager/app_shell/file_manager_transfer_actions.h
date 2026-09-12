// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Apps::FileManager {
class ClipboardController;
class MutationController;
class NavigationController;

// GUI-thread composition, called once after installing the action catalog.
// Borrows all objects; Qt disconnects on either destruction. Keeps the
// clipboard and properties action availability synced with selection count,
// clipboard content, the mutation busy slot, and -- since S5 -- disabled
// while navigation reports a remote (smb/sftp) location active.
void bindFileManagerTransferActions(AppShell::ApplicationCoordinator &coordinator,
                                    NavigationController &navigation,
                                    ClipboardController &clipboard,
                                    MutationController &mutation);
} // namespace QindaQt::Apps::FileManager
