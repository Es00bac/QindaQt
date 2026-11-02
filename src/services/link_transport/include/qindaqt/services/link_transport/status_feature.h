// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/services/link_transport/feature.h>

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

namespace QindaQt::Link {

// Receives phone `status` frames and keeps the latest validated snapshot per
// device: {"battery":{level,charging,plugged},"network":{type,signal}}.
class StatusFeature final : public QObject, public FeatureHandler {
    Q_OBJECT
public:
    using QObject::QObject;
    [[nodiscard]] QString ns() const override { return QStringLiteral("status"); }
    void handle(Session &session, const Envelope &envelope) override;
    void sessionClosed(const QString &deviceId) override;
    [[nodiscard]] QJsonObject snapshot(const QString &deviceId) const { return m_status.value(deviceId); }

Q_SIGNALS:
    void statusChanged(const QString &deviceId);

private:
    QHash<QString, QJsonObject> m_status;
};

} // namespace QindaQt::Link
