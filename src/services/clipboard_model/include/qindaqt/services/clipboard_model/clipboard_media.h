// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_types.h>

#include <QtCore/QString>

namespace QindaQt::Services::ClipboardModel {

struct MediaCanonicalization {
    ClipboardError error = ClipboardError::None;
    QString canonical;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

// Lowercases, trims, and shape-checks one producer media type string.
// Accepted forms are a single "type/subtype" pair or a bare vendor marker
// token, both restricted to [a-z0-9+._-] after lowering. Parameters,
// whitespace inside the token, wildcards, and over-long strings are refused
// rather than sanitized; the caller decides how to surface the refusal.
[[nodiscard]] MediaCanonicalization canonicalizeMediaType(const QString &mediaType,
                                                          int maxLength = kMaxMediaTypeLength);

[[nodiscard]] bool isCanonicalMediaType(const QString &mediaType,
                                        int maxLength = kMaxMediaTypeLength);

// AGENT-CONTRACT: allowlist classification shared by admission and codecs.
// Keep the exact tables in clipboard_media.cpp in sync with the wiki page;
// adding an entry is a documented policy change, not a code cleanup.
[[nodiscard]] MediaClass classifyMediaType(const QString &canonicalMediaType);

// Replaces control/format characters with spaces and clamps to maxCodeUnits.
// The label is producer-supplied metadata and must never be trusted as a
// display string without this pass.
[[nodiscard]] QString sanitizeSourceLabel(const QString &label, int maxCodeUnits);

} // namespace QindaQt::Services::ClipboardModel
