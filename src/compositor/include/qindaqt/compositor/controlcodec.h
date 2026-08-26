// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "controltypes.h"

#include <QByteArray>

#include <optional>

namespace QindaQt::Compositor {

class ControlCodec final
{
public:
    [[nodiscard]] static std::optional<ControlRequest> parseRequest(
        const QJsonObject &object,
        ControlFailure *failure = nullptr);
    [[nodiscard]] static QJsonObject replyToJson(const ControlReply &reply);
    [[nodiscard]] static QJsonObject capabilities();
    [[nodiscard]] static QByteArray compactJson(const QJsonObject &object);
};

} // namespace QindaQt::Compositor
