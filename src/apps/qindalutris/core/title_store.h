// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library_store.h"
#include "title_record.h"

#include <QString>
#include <QVector>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: titles-v1.json under the QindaLutris config root
// ($XDG_CONFIG_HOME/qindaqt/qindalutris, ADR-0275 section 6), in the exact
// posture of LibraryStore (ADR-0231 on ADR-0198):
//   { "version": 1, "titles": [ { <every TitleRecord key> }, ... ] }
// Every key is required and no other key is allowed, at both levels. A read
// refuses the WHOLE document -- leaving no titles loaded, never a partial
// set -- for a newer or unknown version, an oversized file, a symlinked
// path, an unknown key, a wrong type, an out-of-set enum, a duplicate id,
// or any record failing validateTitleRecord (an empty or alias protonBuild
// included). A write refuses (WriteFailed) anything a read would refuse,
// and commits atomically through QSaveFile. The root is injected; the
// object holds no state beyond it and is cheap to construct per use.
// Single-threaded use per root; concurrent writers are last-commit-wins.
class TitleStore final {
public:
  using Error = LibraryStore::Error;

  explicit TitleStore(QString configRoot);

  [[nodiscard]] QString titlesPath() const;
  [[nodiscard]] QVector<TitleRecord> readTitles(Error *error) const;
  [[nodiscard]] Error writeTitles(const QVector<TitleRecord> &records) const;

private:
  QString m_root;
};

} // namespace QindaQt::QindaLutris
