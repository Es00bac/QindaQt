// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Apps::FileManager {
class MutationController;
class NavigationController;

// GUI-thread composition, called once after installing the action catalog.
// Borrows all objects; Qt disconnects on either destruction.
//
// AGENT-GUARD: current-folder mutation actions (new folder, rename, copy,
// move, trash) disable while a remote (smb/sftp) location is active --
// remote entries carry no local mutation identity and this slice never
// mutates a remote location. Undo, Restore Last, Empty Trash, and Cancel
// operate on the local Trash and last-operation history independent of the
// current folder (ADR-0137 Consequences), so they are gated only by the
// mutation-busy slot -- never by NavigationController::remoteActive.
void bindFileManagerMutationActions(AppShell::ApplicationCoordinator &coordinator,
                                    NavigationController &navigation,
                                    MutationController &mutation);
} // namespace QindaQt::Apps::FileManager
