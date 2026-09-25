// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library_store.h"

#include <QString>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: app-wide QindaLutris preferences, app-local like the other
// documents (ADR-0231 / ADR-0198): preferences-v1.json under the injected
// config root. Exact schema, bounded, symlink-refusing, atomic, refused WHOLE
// on anything out of set (then defaults stand).
//
// defaultProtonBuild is the build NAME the user chose in the Proton manager
// for NEW installs (ADR-0275 section 2). Empty means "use the compatibility
// database's recommendation, else the newest system build". It never
// changes an existing title's pin.
struct Preferences final {
  QString defaultProtonBuild;

  friend bool operator==(const Preferences &, const Preferences &) = default;
};

class PreferencesStore final {
public:
  using Error = LibraryStore::Error;

  explicit PreferencesStore(QString configRoot);

  [[nodiscard]] QString path() const;
  [[nodiscard]] Preferences read(Error *error) const;
  [[nodiscard]] Error write(const Preferences &preferences) const;

private:
  QString m_root;
};

} // namespace QindaQt::QindaLutris
