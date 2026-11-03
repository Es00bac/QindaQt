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
    Q_PROPERTY(QVariantMap lan MEMBER m_lan NOTIFY changed)
    Q_PROPERTY(int batteryLevel MEMBER m_battery NOTIFY changed)
    Q_PROPERTY(bool charging MEMBER m_charging NOTIFY changed)
    Q_PROPERTY(QString network MEMBER m_network NOTIFY changed)
    Q_PROPERTY(QVariantMap perm MEMBER m_perm NOTIFY changed)
    // Input and macros: same names and types as LinkBridge.h.
    Q_PROPERTY(QString inputState MEMBER m_inputState NOTIFY inputChanged)
    Q_PROPERTY(QVariantList macros MEMBER m_macros NOTIFY inputChanged)
    Q_PROPERTY(QString macroRunning MEMBER m_macroRunning NOTIFY inputChanged)
    Q_PROPERTY(QString macroRunState MEMBER m_macroRunState NOTIFY inputChanged)
    Q_PROPERTY(QString macroReason MEMBER m_macroReason NOTIFY inputChanged)
    Q_PROPERTY(QString macroLast MEMBER m_macroLast NOTIFY inputChanged)
    Q_PROPERTY(double sensitivity MEMBER m_sensitivity NOTIFY inputChanged)

public:
    static QStringList scenarios();
    void setScenario(const QString &name);

    // Input scenarios the preview grabs: active, pending, denied, locked, running, failed, empty.
    static QStringList inputScenarios();
    void setInputScenario(const QString &name);

    Q_INVOKABLE void inputBegin() { ++m_begins; }
    Q_INVOKABLE void inputEnd() { ++m_ends; }
    Q_INVOKABLE bool touch(int fingers, double, double, double, double) { m_touches += fingers > 0; return false; }
    Q_INVOKABLE void click(const QString &) {}
    Q_INVOKABLE void sendKey(const QString &, int) {}
    Q_INVOKABLE void sendChar(int, int) {}
    Q_INVOKABLE void sendText(const QString &) {}
    Q_INVOKABLE void macroRun(const QString &id) { m_macroRunning = id; m_macroRunState = QStringLiteral("running"); emit inputChanged(); }
    Q_INVOKABLE void macroCancel() { m_macroRunning.clear(); m_macroRunState = QStringLiteral("cancelled"); emit inputChanged(); }
    Q_INVOKABLE void haptic() {}

    Q_INVOKABLE void openSettings(const QString &key) { m_lastSettings = key; }
    Q_INVOKABLE void pair();
    Q_INVOKABLE void pairAgain();
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();
    Q_INVOKABLE void forget();
    Q_INVOKABLE QString setManualAddress(const QString &address);
    Q_INVOKABLE void selectDesktop(const QString &id);

signals:
    void changed();
    void inputChanged();

private:
    QString m_phase = QStringLiteral("disconnected");
    QString m_desktopName, m_code, m_message, m_pairedName, m_transport, m_network = QStringLiteral("wifi");
    QString m_lastSettings;
    bool m_paired = false;
    bool m_charging = false;
    int m_battery = -1;
    QVariantMap m_perm;
    QVariantMap m_lan;
    QString m_inputState = QStringLiteral("active");
    QVariantList m_macros;
    QString m_macroRunning, m_macroRunState, m_macroReason, m_macroLast;
    double m_sensitivity = 1.6;
    int m_begins = 0, m_ends = 0, m_touches = 0;
};
