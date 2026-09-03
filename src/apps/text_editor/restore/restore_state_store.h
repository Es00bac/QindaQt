// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::Apps::TextEditor {

struct RestoreState final {
  QStringList paths;
  int activeIndex = -1;

  [[nodiscard]] bool operator==(const RestoreState &) const = default;
};

enum class RestoreStateError {
  None,
  Absent,
  InvalidRoot,
  ReadFailed,
  TooLarge,
  Malformed,
  WriteFailed,
};

struct RestoreLoadResult final {
  std::optional<RestoreState> state;
  RestoreStateError error = RestoreStateError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return state.has_value(); }
};

struct RestoreWriteResult final {
  RestoreStateError error = RestoreStateError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == RestoreStateError::None; }
};

// Owns only the versioned path inventory. The injected root is the directory
// beneath XDG_STATE_HOME selected by composition; this class never discovers
// HOME, Settings1, document content, or UI. Every successful write is an
// atomic same-directory replacement and contains paths plus active index only.
class RestoreStateStore final {
public:
  static constexpr qint64 maximumBytes = 64 * 1024;
  static constexpr int maximumPaths = 32;
  static constexpr int maximumPathLength = 4096;

  explicit RestoreStateStore(QString stateDirectory);

  [[nodiscard]] QString filePath() const;
  [[nodiscard]] RestoreLoadResult load() const;
  [[nodiscard]] RestoreWriteResult store(const RestoreState &state) const;
  [[nodiscard]] RestoreWriteResult clear() const;

private:
  [[nodiscard]] static bool validate(const RestoreState &state,
                                     QString *diagnostic);

  QString m_stateDirectory;
};

} // namespace QindaQt::Apps::TextEditor
