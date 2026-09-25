// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "preferences.h"

#include <QString>

namespace QindaQt::Apps::FileManager {

enum class PreferencesError {
  None,
  Absent,
  InvalidRoot,
  ReadFailed,
  TooLarge,
  Malformed,
  WriteFailed,
};

struct PreferencesLoadResult final {
  Preferences preferences;
  PreferencesError error = PreferencesError::None;
  QString diagnostic;
  // True when the values came from a preferences-v1 document (ADR-0270): the
  // next store() writes them, with everything v1 lacked at its default, as
  // preferences-v2.
  bool migrated = false;

  [[nodiscard]] bool ok() const { return error == PreferencesError::None; }
};

struct PreferencesWriteResult final {
  PreferencesError error = PreferencesError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == PreferencesError::None; }
};

// AGENT-CONTRACT: owns only the versioned preferences documents beneath the
// injected state directory, over the shared symlink-refusing StateFile
// primitive (ADR-0090 pattern, same as BookmarksStore and
// NetworkLocationsStore). It reads no environment and applies nothing.
//
// AGENT-CONTRACT (ADR-0270): the current document is `preferences-v2`.
// load() reads it; only when it is Absent does load() read the ADR-0198
// `preferences-v1` document and migrate it. store() always writes v2 and
// never touches v1, so an older build keeps reading its own file. A v2
// document that exists but is refused is reported as such -- load() never
// falls back to v1 then, which would silently resurrect old settings.
//
// AGENT-CONTRACT: the readers demand exact key sets (preferences_json.h), so
// a document written by a newer schema is refused rather than partly
// understood. Adding a preference means `preferences-v3` plus a migration,
// not a tolerant reader.
class PreferencesStore final {
public:
  // v2 carries up to Preferences::maximumFolderViews remembered folders.
  static constexpr qint64 maximumBytes = 128 * 1024;
  static constexpr qint64 maximumVersion1Bytes = 16 * 1024;

  explicit PreferencesStore(QString stateDirectory);

  // The preferences-v2 document's path.
  [[nodiscard]] QString filePath() const;
  // Absent is a clean first-run result: ok() is false, diagnostic is empty,
  // and `preferences` holds the documented defaults.
  [[nodiscard]] PreferencesLoadResult load() const;
  [[nodiscard]] PreferencesWriteResult store(const Preferences &preferences) const;

private:
  QString m_stateDirectory;
};

} // namespace QindaQt::Apps::FileManager
