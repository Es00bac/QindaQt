// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "runtime/folder_navigations.h"

#include <QString>

#include <memory>

namespace QindaQt::AppShell {
class ApplicationCoordinator;
} // namespace QindaQt::AppShell

namespace QindaQt::Apps::FileManager {

class ApplicationsController;
class ClipboardController;
class MutationController;
class NavigationController;
class PreviewProvider;
class SearchController;

// Production NavigationController: the local lister and launcher behind the
// Applications (ADR-0262) and Recents (ADR-0272) places' seams, the KIO
// network seams, and its own
// ApplicationsPlaceOrder as a child. main.cpp builds the window's first
// controller with it and FolderNavigations every later one (ADR-0271).
// `applications` must outlive the result.
[[nodiscard]] std::unique_ptr<NavigationController>
composeNavigation(ApplicationsController &applications);

// What follows the controller the user works in (ADR-0271); all borrowed,
// all must outlive the FolderNavigations built from them.
struct WindowSeams final {
  AppShell::ApplicationCoordinator &coordinator;
  MutationController &mutation;
  ClipboardController &clipboard;
  SearchController &search;
  // The engine's two preview caches (ADR-0111/0270), fenced by the active
  // controller's listing generation.
  PreviewProvider &previews;
  PreviewProvider &gallery;
  QString trashFilesDirectory;
};

// The window's FolderNavigations over `first`: new controllers come from
// composeNavigation(), and the active one drives every AppShell action state,
// receives search results and fences the previews. Binds `first` at once.
[[nodiscard]] std::unique_ptr<FolderNavigations>
composeFolderNavigations(NavigationController &first, ApplicationsController &applications,
                         const WindowSeams &seams);

} // namespace QindaQt::Apps::FileManager
