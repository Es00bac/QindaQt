// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "preferences.h"

#include <QJsonObject>
#include <QString>

#include <optional>

namespace QindaQt::Apps::FileManager::PreferencesJson {

// The preferences documents as JSON, and nothing else: no I/O, no file
// names, no migration policy (PreferencesStore owns those). Pure functions.
//
// AGENT-CONTRACT: the reader demands the exact key set of the version it is
// asked for -- at the top level, in every Details column and in every
// remembered folder -- so a document written by a newer schema is refused
// rather than partly understood. A document whose values are out of range
// is refused as a whole: silently repairing one field would hand the user a
// configuration they never chose while pretending the rest survived.

// {"version": 2, "preferences": {...}} for `preferences`, which must be valid.
[[nodiscard]] QJsonObject encode(const Preferences &preferences);

// Reads a whole document of `version` 1 (ADR-0198) or 2 (ADR-0270). A v1
// document yields the v2 defaults for everything v1 did not have. On
// refusal, returns nullopt and a short diagnostic.
[[nodiscard]] std::optional<Preferences> decode(const QJsonObject &document, int version,
                                                QString *diagnostic);

} // namespace QindaQt::Apps::FileManager::PreferencesJson
