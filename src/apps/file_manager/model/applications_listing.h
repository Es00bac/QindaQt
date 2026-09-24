// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "file_manager_types.h"

#include "qindaqt/application_catalog/application_directory_scan.h"
#include "qindaqt/application_catalog/category_tree.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

// Pure projections of the shared application catalog into the File Manager's
// Applications place (ADR-0262): one DirectoryEntry row per application for
// the ordinary views, and the fields Get Info shows. No I/O, no state; the
// ApplicationsController owns the scan these read.
namespace QindaQt::Apps::FileManager::ApplicationsListing {

// Entry id -> the label of the entry's primary category group ("Development",
// "Sound & Video", ...), read from the shared catalog tree so the File
// Manager never keeps a second category mapping (ADR-0164).
[[nodiscard]] QHash<QString, QString> categoryLabels(
    const QindaQt::ApplicationCatalog::CategoryNode &tree);

// Why this application cannot start from a standalone File Manager window,
// or empty when its planned argv is a plain process. In chooser mode every
// entry is choosable (the compositor owns the full launch facility, ADR-0165),
// so this is empty there too. The views dim a row with a note; activating it
// still tries the compositor first, which starts it while docked (ADR-0172).
[[nodiscard]] QString standaloneLimitation(
    const QindaQt::ApplicationCatalog::ScannedApplication &application,
    bool chooserMode);

// One Applications-place row: the display name, the virtual row path
// (ApplicationsLocation::entryPath), the theme icon, the category as its
// Kind, and the limitation note. Size and modification time stay unknown.
[[nodiscard]] DirectoryEntry row(
    const QindaQt::ApplicationCatalog::ScannedApplication &application,
    const QString &category, bool chooserMode);

// Get Info fields: name, genericName, comment, category, categories (the
// entry's raw XDG list), command (the planned argv, display only), iconName,
// desktopFilePath, id and note. Unknown text fields are empty strings.
[[nodiscard]] QVariantMap describe(
    const QindaQt::ApplicationCatalog::ScannedApplication &application,
    const QString &category, bool chooserMode);

} // namespace QindaQt::Apps::FileManager::ApplicationsListing
