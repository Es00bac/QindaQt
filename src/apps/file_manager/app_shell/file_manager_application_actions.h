// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace QindaQt::AppShell { class ApplicationCoordinator; }
namespace QindaQt::Apps::FileManager {
class ClipboardController;
class NavigationController;

// GUI-thread composition, called once after installing the action catalog.
// Borrows all objects; Qt disconnects on either destruction. ADR-0262: the
// Applications place's own actions -- Open (any selection), Show Desktop Entry
// File (exactly one selected row) and Group by Category -- are enabled only
// while that place is the visible folder view; Group by Category is checked
// while the window sorts by category there, because grouping IS the category
// sort (ListingOrder) plus the views' section breaks.
void bindFileManagerApplicationActions(AppShell::ApplicationCoordinator &coordinator,
                                       NavigationController &navigation,
                                       ClipboardController &clipboard);
} // namespace QindaQt::Apps::FileManager
