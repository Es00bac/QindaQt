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

  [[nodiscard]] bool ok() const { return error == PreferencesError::None; }
};

struct PreferencesWriteResult final {
  PreferencesError error = PreferencesError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == PreferencesError::None; }
};

// AGENT-CONTRACT: owns only the versioned `preferences-v1` document beneath
// the injected state directory, over the shared symlink-refusing StateFile
// primitive (ADR-0090 pattern, same as BookmarksStore and
// NetworkLocationsStore). It reads no environment and applies nothing.
//
// AGENT-CONTRACT: the reader demands an exact key set, so a document written
// by a newer schema is refused rather than partly understood. Adding a
// preference means `preferences-v2` plus a migration, not a tolerant reader.
// A document whose values are out of range is Malformed as a whole: silently
// repairing one field would hand the user a configuration they never chose
// while pretending the rest survived.
class PreferencesStore final {
public:
  static constexpr qint64 maximumBytes = 16 * 1024;

  explicit PreferencesStore(QString stateDirectory);

  [[nodiscard]] QString filePath() const;
  // Absent is a clean first-run result: ok() is false, diagnostic is empty,
  // and `preferences` holds the documented defaults.
  [[nodiscard]] PreferencesLoadResult load() const;
  [[nodiscard]] PreferencesWriteResult store(const Preferences &preferences) const;

private:
  QString m_stateDirectory;
};

} // namespace QindaQt::Apps::FileManager
