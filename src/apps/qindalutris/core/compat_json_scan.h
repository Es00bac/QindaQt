// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Module-private: the strict JSON pre-pass for compat-db-v1 documents.

#include <QByteArray>

namespace QindaQt::QindaLutris::CompatJson {

inline constexpr int kMaxDepth = 512;

// AGENT-NOTE: QJsonDocument is lenient where Python's json module is not --
// it skips a UTF-8 BOM, accepts number forms such as "1." and ".1e1", and
// silently keeps the last of two duplicate object keys. The document must be
// judged identically on both sides (AGENT-CONTRACT in compat_db_parse.cpp
// and qlcompat/schema.py), so the raw bytes are checked here first and
// QJsonDocument only ever sees text both parsers read the same way.
//
// True when `bytes` is exactly one RFC 8259 JSON text: strict UTF-8 (no
// overlong forms, no encoded surrogates, nothing above U+10FFFF), no BOM,
// only space/tab/LF/CR as whitespace, RFC number and string grammar (no raw
// control characters, only the eight short escapes and \uXXXX), no object
// with two equal keys (compared after unescaping, as UTF-16), and nesting
// no deeper than kMaxDepth. Pure; no allocation beyond per-object key sets.
[[nodiscard]] bool isStrictJson(const QByteArray &bytes);

} // namespace QindaQt::QindaLutris::CompatJson
