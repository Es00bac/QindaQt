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
    // Input and macros (org.qindaqt.link.input / .macro; FakeBridge mirrors these). inputState is one of
    // off, offline, pending, active, denied, unavailable, locked, busy, revoked. macros is [{id, name}].
    Q_PROPERTY(QString inputState READ inputState NOTIFY inputChanged)
    Q_PROPERTY(QVariantList macros READ macros NOTIFY inputChanged)
    Q_PROPERTY(QString macroRunning READ macroRunning NOTIFY inputChanged)
    Q_PROPERTY(QString macroRunState READ macroRunState NOTIFY inputChanged)
    Q_PROPERTY(QString macroReason READ macroReason NOTIFY inputChanged)
    Q_PROPERTY(QString macroLast READ macroLast NOTIFY inputChanged)
    Q_PROPERTY(double sensitivity READ sensitivity WRITE setSensitivity NOTIFY sensitivityChanged)

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

    QString inputState() const { return m_input.value(QStringLiteral("state"), QStringLiteral("offline")).toString(); }
    QVariantList macros() const { return m_macro.value(QStringLiteral("macros")).toList(); }
    QString macroRunning() const { return m_macro.value(QStringLiteral("running")).toString(); }
    QString macroRunState() const { return m_macro.value(QStringLiteral("runState")).toString(); }
    QString macroReason() const { return m_macro.value(QStringLiteral("reason")).toString(); }
    QString macroLast() const { return m_macro.value(QStringLiteral("last")).toString(); }
    double sensitivity() const { return m_sensitivity; }
    void setSensitivity(double value);

    // Remote input. All calls are non-blocking and only queue work for the engine thread.
    Q_INVOKABLE void inputBegin();
    Q_INVOKABLE void inputEnd();
    // One touch frame in pixels (second point is ignored with fewer than two fingers); true = give haptic feedback.
    Q_INVOKABLE bool touch(int fingers, double x0, double y0, double x1, double y1);
    Q_INVOKABLE void click(const QString &button);
    Q_INVOKABLE void sendKey(const QString &name, int mods);
    Q_INVOKABLE void sendChar(int codePoint, int mods);
    Q_INVOKABLE void sendText(const QString &text);
    Q_INVOKABLE void macroRun(const QString &id);
    Q_INVOKABLE void macroCancel();
    Q_INVOKABLE void haptic();

    Q_INVOKABLE void openSettings(const QString &key);
    Q_INVOKABLE void pair();
    Q_INVOKABLE void pairAgain();
    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();
    Q_INVOKABLE void forget();

signals:
    void changed();
    void inputChanged();
    void sensitivityChanged();

private:
    void poll();
    void pollDevice();
    void pollInput();
    void call(const char *method);
    QVariantMap m_s;
    QVariantMap m_dev;
    QVariantMap m_input, m_macro;
    double m_sensitivity = 1.6;
    QTimer m_fast; // 150 ms input-state poll while remote input is wanted
    int m_tick = 0;
    QTimer m_timer;
};
