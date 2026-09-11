// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/night_light/night_light_values.h>

#include <QtCore/QDateTime>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusPendingCallWatcher>

namespace QindaQt::Services::NightLight {

// One transition window as KWin reports it. A null dateTime means "no
// transition" (KWin reports epoch 0, e.g. in constant mode).
struct NightLightTransition {
    QDateTime dateTime;
    quint32 durationMilliseconds = 0;

    friend bool operator==(const NightLightTransition &,
                           const NightLightTransition &) = default;
};

// Complete live truth from the KWin nightlight plugin. There is deliberately
// no partial form: a frame that fails validation replaces nothing.
struct NightLightStatus {
    bool available = false;
    bool enabled = false;
    bool running = false;
    bool inhibited = false;
    Mode mode = Mode::DarkLight;
    bool daylight = true;
    int currentTemperatureKelvin = kNeutralTemperatureKelvin;
    int targetTemperatureKelvin = kNeutralTemperatureKelvin;
    NightLightTransition previousTransition;
    NightLightTransition scheduledTransition;

    friend bool operator==(const NightLightStatus &,
                           const NightLightStatus &) = default;
};

// Live-state port over `org.kde.KWin.NightLight`. Implementations must keep
// every D-Bus detail private, publish complete validated frames only, and
// treat transport loss as unavailable truth rather than stale truth
// (fail closed, ADR-0136).
class NightLightStatePort : public QObject {
    Q_OBJECT

public:
    explicit NightLightStatePort(QObject *parent = nullptr);
    ~NightLightStatePort() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    // Previews one temperature for KWin's short preview window. Out-of-range
    // temperatures are refused locally without touching the bus.
    virtual void preview(int temperatureKelvin) = 0;
    virtual void stopPreview() = 0;

Q_SIGNALS:
    // Published only for complete validated frames, including the explicit
    // unavailable frame published when the service disappears.
    void statusChanged(
        const QindaQt::Services::NightLight::NightLightStatus &status);
    // A read or change notification failed validation, or the bus call for a
    // refresh failed. The last complete status stays published.
    void degraded(const QString &reason);
};

// Production implementation over an injected connection. Never touches the
// session bus passed by the caller other than through this object's calls;
// tests register their own fake `org.kde.KWin.NightLight` on a private bus.
class QtNightLightStatePort final : public NightLightStatePort {
    Q_OBJECT

public:
    explicit QtNightLightStatePort(const QDBusConnection &connection,
                                   QObject *parent = nullptr);
    ~QtNightLightStatePort() override;

    void start() override;
    void stop() override;
    void preview(int temperatureKelvin) override;
    void stopPreview() override;

private:
    void publishStatus(const NightLightStatus &status);
    void beginRefresh(bool serviceAppearance);
    void finishRefresh(QDBusPendingCallWatcher *watcher, bool serviceAppearance);

    class Private;
    std::unique_ptr<Private> d;

    // AGENT-NOTE: QtDBus only delivers into declared slots (no functor
    // overloads), so bus events land here. Both slots are always invoked
    // through Qt::QueuedConnection: they run inside the connection's message
    // dispatch, and a blocking GetAll issued directly would deadlock until
    // timeout because its reply can never be dispatched on the re-entered
    // connection.
private Q_SLOTS:
    // The service appeared (or a change hint arrived) and the complete frame
    // must be re-read. A failure keeps the last complete status and reports
    // degradation.
    void refreshChangeHint();
    // The service name reappeared after an absence. A failure publishes the
    // explicit unavailable frame so consumers cannot mistake a hostile
    // producer for a working one.
    void refreshServiceFrame();
    void handlePropertiesChanged(const QString &interfaceName,
                                 const QVariantMap &changedProperties,
                                 const QStringList &invalidatedProperties);
};

} // namespace QindaQt::Services::NightLight
