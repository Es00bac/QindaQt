// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_protocol/envelope.h>

#include <QtCore/QRegularExpression>

namespace QindaQt::Link {

bool isValidFrameType(const QString &t)
{
    static const QRegularExpression pattern(
        QStringLiteral("^[a-z][a-z0-9]*(\\.[a-z][a-z0-9-]*)*$"));
    return t.size() >= 1 && t.size() <= 64 && pattern.match(t).hasMatch();
}

Envelope makeError(const QString &code, const QString &message, qint64 re)
{
    Envelope e;
    e.t = QStringLiteral("error");
    e.re = re;
    e.b = QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("message"), message.left(512)}};
    return e;
}

Envelope makeBye(const QString &reason)
{
    Envelope e;
    e.t = QStringLiteral("bye");
    e.b = QJsonObject{{QStringLiteral("reason"), reason}};
    return e;
}

} // namespace QindaQt::Link
