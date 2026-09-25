// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

class QObject;

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Apps::FileManager {
class ClipboardController;
class NavigationController;

// GUI-thread composition, called once after installing the action catalog.
// Borrows all objects; Qt disconnects on either destruction. ADR-0262: the
// Applications place's own actions -- Show Desktop Entry File (exactly one
// selected row) and Group by Category -- are enabled only while that place
// is the visible folder view (Open became the shared file.open in ADR-0269,
// bound in file_manager_item_actions); Group by Category is checked
// while the window sorts by category there, because grouping IS the category
// sort (ListingOrder) plus the views' section breaks.
// ADR-0271: `context`, when given, is the connections' context object in
// place of `coordinator`; destroying it unbinds (runtime/folder_navigations.h).
void bindFileManagerApplicationActions(AppShell::ApplicationCoordinator &coordinator,
                                       NavigationController &navigation,
                                       ClipboardController &clipboard,
                                       QObject *context = nullptr);
} // namespace QindaQt::Apps::FileManager
