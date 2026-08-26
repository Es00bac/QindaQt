// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation/presentation_access_token.h"

#include <QString>

#include <optional>

namespace QindaQt::Services::NotificationPresentation {

enum class TokenChannelStatus {
    Received,
    InvalidDescriptor,
    ReadFailed,
    ReadTimedOut,
    InvalidRecord,
};

struct TokenChannelReadResult final {
    TokenChannelStatus status = TokenChannelStatus::InvalidDescriptor;
    std::optional<PresentationAccessToken> token;
    QString message;

    [[nodiscard]] bool ok() const noexcept
    {
        return status == TokenChannelStatus::Received && token.has_value();
    }
};

// Consumes a one-record Linux descriptor channel. Every call closes its
// descriptor on success and failure; messages never include secret contents.
class PresentationTokenChannel final {
public:
    [[nodiscard]] static TokenChannelReadResult readAndClose(int descriptor);
    [[nodiscard]] static bool writeAndClose(
        int descriptor, const PresentationAccessToken &token,
        QString *error = nullptr);
};

[[nodiscard]] QString tokenChannelStatusName(TokenChannelStatus status);

} // namespace QindaQt::Services::NotificationPresentation
