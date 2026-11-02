#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>

// AGENT-CONTRACT: the only C++ <-> Java seam of the phone app. It calls the
// static methods of org.qindaqt.link.android.LinkBridge by name and polls
// stateJson() twice a second; no Java -> C++ callbacks exist, so the engine
// never depends on the UI being alive. Ownership: lives on the GUI thread.
class LinkBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString desktopName READ desktopName NOTIFY changed)
    Q_PROPERTY(QString code READ code NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool paired READ paired NOTIFY changed)
    Q_PROPERTY(QString pairedName READ pairedName NOTIFY changed)
    // Device facts for the UI (additive; DeviceFacts.java is the other side). transport is "usb",
    // "wifi" or "" and is "usb" while a session is up until the engine reports its own.
    // perm maps notificationAccess|sms|contacts|postNotifications|batteryOptimisation to
    // "granted" | "denied" | "later".
    Q_PROPERTY(QString transport READ transport NOTIFY changed)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY changed)
    Q_PROPERTY(bool charging READ charging NOTIFY changed)
    Q_PROPERTY(QString network READ network NOTIFY changed)
    Q_PROPERTY(QVariantMap perm READ perm NOTIFY changed)

public:
    explicit LinkBridge(QObject *parent = nullptr);

    QString phase() const { return m_s.value(QStringLiteral("phase")).toString(); }
    QString desktopName() const { return m_s.value(QStringLiteral("desktopName")).toString(); }
    QString code() const { return m_s.value(QStringLiteral("code")).toString(); }
    QString message() const { return m_s.value(QStringLiteral("message")).toString(); }
    bool paired() const { return m_s.value(QStringLiteral("paired")).toBool(); }
    QString pairedName() const { return m_s.value(QStringLiteral("pairedName")).toString(); }

    QString transport() const;
    int batteryLevel() const { return m_dev.value(QStringLiteral("batteryLevel"), -1).toInt(); }
    bool charging() const { return m_dev.value(QStringLiteral("charging")).toBool(); }
    QString network() const { return m_dev.value(QStringLiteral("network")).toString(); }
    QVariantMap perm() const { return m_dev.value(QStringLiteral("perm")).toMap(); }

    Q_INVOKABLE void openSettings(const QString &key);
    Q_INVOKABLE void pair();
    Q_INVOKABLE void pairAgain();
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();
    Q_INVOKABLE void forget();

signals:
    void changed();

private:
    void poll();
    void pollDevice();
    void call(const char *method);
    QVariantMap m_s;
    QVariantMap m_dev;
    int m_tick = 0;
    QTimer m_timer;
};
