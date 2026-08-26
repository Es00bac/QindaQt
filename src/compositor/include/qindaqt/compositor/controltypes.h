// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace QindaQt::Compositor {

struct ProtocolVersion final
{
    static constexpr quint16 CurrentMajor = 1;
    static constexpr quint16 CurrentMinor = 0;

    quint16 major = CurrentMajor;
    quint16 minor = CurrentMinor;

    friend bool operator==(const ProtocolVersion &, const ProtocolVersion &) = default;
};

enum class ReplyStatus {
    Committed,
    Rejected,
    Conflict,
};

struct ControlFailure final
{
    QString code;
    QString message;
    qsizetype operationIndex = -1;
};

struct ControlRequest final
{
    ProtocolVersion protocol;
    QString transactionId;
    QString containerId;
    quint64 expectedRevision = 0;
    QVector<QJsonObject> operations;
};

struct ControlReply final
{
    ProtocolVersion protocol;
    QString transactionId;
    QString containerId;
    ReplyStatus status = ReplyStatus::Rejected;
    quint64 revision = 0;
    QJsonObject snapshot;
    ControlFailure failure;

    [[nodiscard]] bool committed() const noexcept { return status == ReplyStatus::Committed; }
};

} // namespace QindaQt::Compositor
