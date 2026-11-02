// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantMap>

// AGENT-CONTRACT: stands in for phone/app/LinkBridge in the desktop preview. It must expose every
// property and invokable the QML uses with the same names and types; LinkBridge.h is the other
// side. Scenarios script the engine states the screens must handle.
class FakeBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase MEMBER m_phase NOTIFY changed)
    Q_PROPERTY(QString desktopName MEMBER m_desktopName NOTIFY changed)
    Q_PROPERTY(QString code MEMBER m_code NOTIFY changed)
    Q_PROPERTY(QString message MEMBER m_message NOTIFY changed)
    Q_PROPERTY(bool paired MEMBER m_paired NOTIFY changed)
    Q_PROPERTY(QString pairedName MEMBER m_pairedName NOTIFY changed)
    Q_PROPERTY(QString transport MEMBER m_transport NOTIFY changed)
    Q_PROPERTY(int batteryLevel MEMBER m_battery NOTIFY changed)
    Q_PROPERTY(bool charging MEMBER m_charging NOTIFY changed)
    Q_PROPERTY(QString network MEMBER m_network NOTIFY changed)
    Q_PROPERTY(QVariantMap perm MEMBER m_perm NOTIFY changed)

public:
    static QStringList scenarios();
    void setScenario(const QString &name);

    Q_INVOKABLE void openSettings(const QString &key) { m_lastSettings = key; }
    Q_INVOKABLE void pair();
    Q_INVOKABLE void pairAgain();
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();
    Q_INVOKABLE void forget();

signals:
    void changed();

private:
    QString m_phase = QStringLiteral("disconnected");
    QString m_desktopName, m_code, m_message, m_pairedName, m_transport, m_network = QStringLiteral("wifi");
    QString m_lastSettings;
    bool m_paired = false;
    bool m_charging = false;
    int m_battery = -1;
    QVariantMap m_perm;
};
