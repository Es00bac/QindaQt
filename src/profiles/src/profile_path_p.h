// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>

namespace QindaQt::Profiles::Internal {

inline QString escapeJsonPointerToken(QString token)
{
    token.replace(QLatin1Char('~'), QStringLiteral("~0"));
    token.replace(QLatin1Char('/'), QStringLiteral("~1"));
    return token;
}

inline QString jsonPointerChild(const QString &parent, const QString &token)
{
    return parent + QLatin1Char('/') + escapeJsonPointerToken(token);
}

inline QString jsonPointerIndex(const QString &parent, qsizetype index)
{
    return parent + QLatin1Char('/') + QString::number(index);
}

} // namespace QindaQt::Profiles::Internal
