// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/shell_launcher/launcher_types.h"

#include <QVector>

#include <optional>

namespace QindaQt::ShellLauncher {

enum class DesktopEntryErrorCode {
  None,
  DocumentTooLarge,
  MissingEntryGroup,
  DuplicateEntryGroup,
  DuplicateActionGroup,
  DuplicateKey,
  InvalidKeyLine,
  InvalidEscape,
  InvalidBoolean,
  InvalidActionId,
  UnsupportedType,
  MissingName,
  UnknownActionReference,
  FieldLimitExceeded,
};

struct DesktopEntryError {
  DesktopEntryErrorCode code = DesktopEntryErrorCode::None;
  int line = 0;
  QString message;

  friend bool operator==(const DesktopEntryError &,
                         const DesktopEntryError &) = default;
};

// The validated subset of one desktop-entry document. `hidden` is true when
// Hidden=true or NoDisplay=true; hidden documents still parse so the catalog
// can distinguish "valid but not shown" from "malformed".
struct ParsedDesktopEntry {
  bool hidden = false;
  QString name;
  QString genericName;
  QString comment;
  QString iconName;
  QStringList categories;
  QStringList keywords;
  QVector<DesktopEntryAction> actions;

  friend bool operator==(const ParsedDesktopEntry &,
                         const ParsedDesktopEntry &) = default;
};

struct DesktopEntryParseResult {
  std::optional<ParsedDesktopEntry> entry;
  DesktopEntryError error;

  bool ok() const { return entry.has_value(); }
};

// Parses caller-supplied keyfile text into validated launcher values.
// AGENT-GUARD: This parser is pure and total over its input string; it must
// never touch the filesystem, environment, or processes. Hostile documents
// (truncated, malformed, oversized) produce typed errors, not exceptions or
// partial entries.
class DesktopEntryParser {
public:
  static DesktopEntryParseResult parse(const QString &text);
};

} // namespace QindaQt::ShellLauncher
