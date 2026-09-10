// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Apps::FileManager {
class ClipboardController;
class MutationController;

// GUI-thread composition, called once after installing the action catalog.
// Borrows all objects; Qt disconnects on either destruction. Keeps the
// clipboard and properties action availability synced with selection count,
// clipboard content, and the mutation busy slot.
void bindFileManagerTransferActions(AppShell::ApplicationCoordinator &coordinator,
                                    ClipboardController &clipboard,
                                    MutationController &mutation);
} // namespace QindaQt::Apps::FileManager
