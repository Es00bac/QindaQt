// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "directory_lister.h"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace QindaQt::Apps::FileManager {

// ADR-0270: the Columns view's other columns -- the folders above the one
// being browsed, and the contents of a folder selected in it. The browsed
// folder itself is always NavigationController's listing; these rows are
// read-only glimpses that a click turns into ordinary navigation.
//
// AGENT-CONTRACT: GUI-thread, stateless between calls. One call is one
// bounded synchronous local listing through the injected lister (the same
// contract as a navigation), so a view asks only for the columns it shows.
// Anything but a readable local folder -- a network URL, the Applications
// place, a missing folder -- yields an empty list rather than an error.
class ColumnListing final : public QObject {
  Q_OBJECT

public:
  explicit ColumnListing(DirectoryListerPtr lister, QObject *parent = nullptr);

  // Light rows, [{name, path, isDirectory, isSymlink, isHidden, iconName}],
  // ordered as the window orders its own listing (ListingOrder keys, no
  // grouping) and without dot names unless `showHidden`.
  Q_INVOKABLE [[nodiscard]] QVariantList children(const QString &path, bool showHidden,
                                                  const QString &sortColumn,
                                                  const QString &sortDirection,
                                                  bool directoriesFirst) const;

private:
  DirectoryListerPtr m_lister;
};

} // namespace QindaQt::Apps::FileManager
