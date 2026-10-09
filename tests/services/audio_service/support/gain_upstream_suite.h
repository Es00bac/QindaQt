// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore/QJsonObject>
#include <QtCore/QMap>
#include <QtCore/QString>

namespace QindaQt::Audio::Test
{
// Checks actual completed Meson observations against the pre-test registry.
// TIMEOUT inside a complete returned suite is distinct from a runner timeout.
[[nodiscard]] bool gainCompleteUpstreamSuite(const QMap<QString, QByteArray> &payloads,
    const QString &root, const QJsonObject &counts, qint64 registryCount, bool failed);
} // namespace QindaQt::Audio::Test
