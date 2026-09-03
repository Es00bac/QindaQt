// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Apps::TextEditor {

constexpr int maximumDocumentTitleLength = 128;
[[nodiscard]] QString sanitizeDocumentTitle(const QString &rawTitle);

} // namespace QindaQt::Apps::TextEditor
