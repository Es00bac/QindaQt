// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui_contract_probe.h"

#include <QObject>
#include <QStringList>

namespace QindaQt::Apps::FileManager {

QString missingUiContractObject(QObject *root) {
  const QStringList requiredObjects = {
      QStringLiteral("newFolderButton"), QStringLiteral("entryListView"),
      QStringLiteral("entryGridView"), QStringLiteral("locationField"),
      QStringLiteral("locationToggleButton"), QStringLiteral("toggleHiddenButton"),
      QStringLiteral("toggleViewModeButton"), QStringLiteral("placesSidebar"),
      QStringLiteral("addBookmarkButton"), QStringLiteral("bookmarkList"),
      QStringLiteral("bookmarkStoreBanner"), QStringLiteral("sortHeader_name"),
      QStringLiteral("sortHeader_size"), QStringLiteral("sortHeader_kind"),
      QStringLiteral("sortHeader_modified"),
      QStringLiteral("mutationProgressCard"), QStringLiteral("mutationFailureCard"),
      QStringLiteral("mutationResultCard"), QStringLiteral("newFolderDialog"),
      QStringLiteral("renameDialog"), QStringLiteral("destinationDialog"),
      QStringLiteral("trashConfirmationDialog"),
      QStringLiteral("emptyTrashConfirmationDialog"),
      QStringLiteral("propertiesDialog"),
      QStringLiteral("filterSubfoldersToggle"),
      // ADR-0194/0195: the network surfaces are part of the installed
      // package's contract, so a packaging change that drops one fails the
      // probe instead of shipping a Network place that opens nothing.
      QStringLiteral("networkHub"), QStringLiteral("connectToServerButton"),
      QStringLiteral("networkLocationList"),
      QStringLiteral("connectToServerDialog"),
      QStringLiteral("connectSchemeBox"), QStringLiteral("connectHostField"),
      QStringLiteral("connectPathField"), QStringLiteral("connectNameField"),
      QStringLiteral("transferQueueBanner"),
      QStringLiteral("transferRefusalBanner"),
      // ADR-0200/0198: the nearby section and the preferences window are part
      // of the installed package's contract too.
      QStringLiteral("nearbyServersSection"), QStringLiteral("preferencesWindow"),
      QStringLiteral("preferencesTabBar"), QStringLiteral("preferencesGeneralPage"),
      QStringLiteral("preferencesViewsPage"),
      QStringLiteral("preferencesNetworkPage"),
      QStringLiteral("preferencesTrashPage"),
      // ADR-0269: the right-click set's own dialogs.
      QStringLiteral("openWithDialog"), QStringLiteral("deleteConfirmationDialog"),
      QStringLiteral("newFileDialog")};
  for (const QString &objectName : requiredObjects) {
    if (!root->findChild<QObject *>(objectName)) {
      return objectName;
    }
  }
  return {};
}

} // namespace QindaQt::Apps::FileManager
