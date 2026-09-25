// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Module-private: the scalar rules of the compat-db-v1 schema, shared by the
// parser and the index builder. Not part of the QindaLutris model's public
// surface -- consumers use compat_db.h.

#include "compat_db.h"

#include <QDateTime>
#include <QString>

#include <optional>

namespace QindaQt::QindaLutris::CompatRules {

// AGENT-CONTRACT: mirrored one-for-one by tools/qindalutris-compat/qlcompat/
// schema.py. Lengths are counted in UTF-16 code units on both sides
// (QString::size here, u16len() there).
inline constexpr const char *kCompatSchemaName = "qindalutris-compat-db";
inline constexpr int kCompatSchemaVersion = 1;

// Bounded string with no C0/C1 control character (QChar::Other_Control ==
// Unicode category Cc). `allowEmpty` false requires at least one unit.
[[nodiscard]] bool isText(const QString &text, int maxChars, bool allowEmpty);

[[nodiscard]] bool isGameId(const QString &text);      // [a-z0-9][a-z0-9._:-]{0,127}
[[nodiscard]] bool isSourceId(const QString &text);    // [a-z0-9][a-z0-9-]{0,63}
[[nodiscard]] bool isUmuId(const QString &text);       // umu-[A-Za-z0-9._-]{1,124}
[[nodiscard]] bool isSteamAppId(const QString &text);  // [1-9][0-9]{0,9}
[[nodiscard]] bool isStoreId(const QString &text);     // [A-Za-z0-9._:-]{1,128}
[[nodiscard]] bool isExeName(const QString &text);     // text<=128, no / or backslash
// [a-z0-9_=.-]{1,64} AND listed in tools/qindalutris-compat/winetricks-verbs.txt.
[[nodiscard]] bool isWinetricksVerb(const QString &text);
[[nodiscard]] bool isHttpsUrl(const QString &text);
[[nodiscard]] bool isUmuStore(const QString &text);
// Case-sensitive environment key allowlist (see compat_db_rules.cpp).
[[nodiscard]] bool isCompatEnvironmentKey(const QString &key);
// KEY=VALUE: text of at most 1089 units, an ASCII identifier key of at most
// 64 on the allowlist, and a value with no '$' or backtick.
[[nodiscard]] bool isCompatEnvironmentAssignment(const QString &line);
[[nodiscard]] QString compatEnvironmentKey(const QString &line);

// "YYYY-MM-DDTHH:MM:SSZ" exactly; invalid QDateTime on any deviation.
[[nodiscard]] QDateTime parseTimestamp(const QString &text);

[[nodiscard]] std::optional<CompatBuildStatus> buildStatusForId(const QString &id);
[[nodiscard]] std::optional<AntiCheatStatus> antiCheatStatusForId(const QString &id);
[[nodiscard]] std::optional<ProtonDbTier> protonDbTierForId(const QString &id);
[[nodiscard]] std::optional<SteamDeckCategory> steamDeckCategoryForId(const QString &id);

} // namespace QindaQt::QindaLutris::CompatRules
