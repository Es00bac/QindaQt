// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QUrl>
#include <QVariantMap>
#include <optional>
namespace QindaQt::Services::Portal {
struct EmailDraft { QUrl uri; QString activationToken; };
// Pure RFC6068 draft construction, preserving literal user text through percent
// encoding. Never sends mail or adds provider-specific attachment query keys.
// Nonempty attachments explicitly fail until a native attachment-aware draft
// contract exists; a generic mailto launch must not claim they were attached.
std::optional<EmailDraft> emailDraft(const QString &parentWindow, const QVariantMap &options);
} // namespace QindaQt::Services::Portal
