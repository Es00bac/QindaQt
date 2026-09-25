// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

class QObject;

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Apps::FileManager {
class ClipboardController;
class MutationController;
class NavigationController;

// GUI-thread composition, called once after installing the action catalog.
// Borrows all objects; Qt disconnects on either destruction. ADR-0269: the
// right-click set's availability from the selection count, the place and
// the mutation busy slot. Open works wherever a selection can be activated
// (Applications rows included); everything else needs a local folder view --
// not the Applications place, not a remote location -- and anything that
// writes waits for the busy slot. Inside the home Trash (`trashFilesDirectory`,
// its files/ folder) Put Back replaces the creating actions; Delete
// Permanently stays. Whether one particular selection fits (a folder for Open
// in New Window, an archive for Extract, files for Open With) is decided by the
// window's FileActions, which sees the entries themselves.
// ADR-0271: `context`, when given, is the connections' context object in
// place of `coordinator`; destroying it unbinds (runtime/folder_navigations.h).
void bindFileManagerItemActions(AppShell::ApplicationCoordinator &coordinator,
                                NavigationController &navigation,
                                ClipboardController &clipboard,
                                MutationController &mutation,
                                const QString &trashFilesDirectory,
                                QObject *context = nullptr);
} // namespace QindaQt::Apps::FileManager
