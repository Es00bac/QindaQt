// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime/navigation_composition.h"

#include "app_shell/file_manager_application_actions.h"
#include "app_shell/file_manager_browsing_actions.h"
#include "app_shell/file_manager_item_actions.h"
#include "app_shell/file_manager_mutation_actions.h"
#include "app_shell/file_manager_transfer_actions.h"
#include "model/applications_controller.h"
#include "model/applications_place.h"
#include "model/applications_place_order.h"
#include "model/launch_intent.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/recents_place.h"
#include "model/search_controller.h"
#include "network/kio_fuse_remote_file_opener.h"
#include "network/kio_network_directory_backend.h"
#include "network/kio_remote_copier.h"
#include "network/kio_remote_folder_creator.h"
#include "network/kio_remote_mover.h"
#include "network/kio_remote_renamer.h"
#include "preview/preview_provider.h"

namespace QindaQt::Apps::FileManager {
namespace {

// A search result set only becomes the visible listing while the controller
// still shows the folder the search started in; otherwise the stale results
// would masquerade as the new location's contents.
void publishSearchResultsInto(SearchController &search, NavigationController &navigation,
                              QObject &context) {
  QObject::connect(&search, &SearchController::searchReady, &context,
                   [&search, &navigation](quint64, const QVector<DirectoryEntry> &entries,
                                          const QString &statusText) {
                     if (search.rootPath() != navigation.currentPath()) {
                       return;
                     }
                     navigation.showGuestListing(entries, statusText);
                   });
}

// Both preview caches follow the listing generation, so a new listing
// cancels every obsolete decode. AGENT-NOTE: only the active controller's
// generation is live, so a background Commander pane that re-lists shows
// icons instead of new previews until the user works in it (ADR-0271).
void followListingGeneration(PreviewProvider &previews, PreviewProvider &gallery,
                             NavigationController &navigation, QObject &context) {
  const auto follow = [&previews, &gallery, &navigation] {
    previews.setGeneration(navigation.listingGeneration());
    gallery.setGeneration(navigation.listingGeneration());
  };
  QObject::connect(&navigation, &NavigationController::entriesChanged, &context, follow);
  follow();
}

} // namespace

std::unique_ptr<NavigationController> composeNavigation(ApplicationsController &applications) {
  ApplicationsController *const place = &applications;
  // Local folders, plus the two virtual places browsed like folders:
  // Applications (ADR-0262), scanned whenever it is listed, and Recents
  // (ADR-0272), read from the desktop's recently-used store.
  auto navigation = std::make_unique<NavigationController>(
      std::make_unique<RecentsDirectoryLister>(
          std::make_unique<ApplicationsDirectoryLister>(
              std::make_unique<LocalDirectoryLister>(),
              [place] { place->refresh(); return place->listing(); }),
          recentlyUsedStorePath()),
      std::make_unique<ApplicationsFileLauncher>(
          std::make_unique<DesktopFileLauncher>(),
          [place](const QString &id) { return place->open(id); }),
      std::make_unique<KioNetworkDirectoryBackend>(),
      std::make_unique<KioFuseRemoteFileOpener>(), std::make_unique<KioRemoteRenamer>(),
      std::make_unique<KioRemoteFolderCreator>(), std::make_unique<KioRemoteCopier>(),
      std::make_unique<KioRemoteMover>());
  // ADR-0262: Applications keeps its own sort (A to Z) apart from folders';
  // built before the first navigation, owned by the controller it watches.
  new ApplicationsPlaceOrder(*navigation, navigation.get());
  return navigation;
}

std::unique_ptr<FolderNavigations>
composeFolderNavigations(NavigationController &first, ApplicationsController &applications,
                         const WindowSeams &seams) {
  ApplicationsController *const place = &applications;
  auto recipe = [place] { return composeNavigation(*place); };
  auto binder = [seams](NavigationController &navigation, QObject &context) {
    bindFileManagerBrowsingActions(seams.coordinator, navigation, &context);
    bindFileManagerTransferActions(seams.coordinator, navigation, seams.clipboard,
                                   seams.mutation, &context);
    bindFileManagerMutationActions(seams.coordinator, navigation, seams.mutation, &context);
    bindFileManagerApplicationActions(seams.coordinator, navigation, seams.clipboard,
                                      &context);
    bindFileManagerItemActions(seams.coordinator, navigation, seams.clipboard, seams.mutation,
                               seams.trashFilesDirectory, &context);
    publishSearchResultsInto(seams.search, navigation, context);
    followListingGeneration(seams.previews, seams.gallery, navigation, context);
  };
  return std::make_unique<FolderNavigations>(first, std::move(recipe), std::move(binder));
}

} // namespace QindaQt::Apps::FileManager
