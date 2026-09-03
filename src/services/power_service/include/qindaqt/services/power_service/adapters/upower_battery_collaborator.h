// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_service/power_collaborators.h>

#include <QtCore/QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusServiceWatcher>

#include <memory>

namespace QindaQt::Power::Upstream {

// AGENT-CONTRACT: Production battery-authority adapter for org.freedesktop.UPower
// on an injected bus connection (the packaged process injects the system bus;
// tests inject a private bus with a fake service). It never contacts any other
// bus, never uses an UPower client library, and never exposes a raw UPower
// object path: public opaque IDs are one-way derivations. Numeric values are
// passed through without range reinterpretation so the existing coordinator
// sanitization and PB-0 aggregate policy remain the sole validators; a wrongly
// typed property, unknown enum ordinal, or unreadable device makes the whole
// domain fail closed as "upower-malformed"/"upower-unavailable". Keyboard
// backlights are intentionally not modeled in this slice and stay empty with
// Unsupported operation outcomes.
class UpowerBatteryCollaborator final : public BatteryCollaborator
{
    Q_OBJECT

public:
    explicit UpowerBatteryCollaborator(const QDBusConnection &upstreamConnection,
                                       QObject *parent = nullptr);
    ~UpowerBatteryCollaborator() override;

    quint64 start() override;
    void stop() override;
    void submitSetKeyboardBrightness(quint64 operationId, const Handle &device,
                                     quint32 value) override;

private:
    struct RefreshCycle;

    void scheduleUnavailable(quint64 generation, const QString &reasonCode);
    [[nodiscard]] bool subscribeServiceSignals();
    void beginRefresh();
    void readServiceProperties(const std::shared_ptr<RefreshCycle> &cycle);
    void enumerateDevices(const std::shared_ptr<RefreshCycle> &cycle);
    void readDeviceProperties(const std::shared_ptr<RefreshCycle> &cycle,
                              const QString &objectPath);
    void tryPublish(const std::shared_ptr<RefreshCycle> &cycle);
    void failRefresh(const std::shared_ptr<RefreshCycle> &cycle,
                     const QString &reasonCode);
    [[nodiscard]] bool acceptReplyOwner(const std::shared_ptr<RefreshCycle> &cycle,
                                        const QString &owner);
    void adoptOwner(const QString &owner);
    void onUpowerOwnerChanged(const QString &name, const QString &oldOwner,
                              const QString &newOwner);
    [[nodiscard]] bool runningGeneration(quint64 generation) const;

    QDBusConnection m_connection;
    QDBusServiceWatcher *m_watcher = nullptr;
    std::shared_ptr<RefreshCycle> m_refresh;
    QString m_activeOwner;
    QString m_lastOwner;
    quint64 m_generation = 0;
    quint64 m_nextGeneration = 0;
    quint64 m_nextRefresh = 0;
    bool m_running = false;
    bool m_signalsSubscribed = false;
    bool m_epochAdvancedForLoss = false;

private Q_SLOTS:
    void onAnyPropertiesChanged(const QDBusMessage &message);
    void onDeviceAdded(const QDBusObjectPath &device);
    void onDeviceRemoved(const QDBusObjectPath &device);
};

} // namespace QindaQt::Power::Upstream
