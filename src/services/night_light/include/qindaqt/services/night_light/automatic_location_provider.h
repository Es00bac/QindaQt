// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QDateTime>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QVariant>

#include <functional>
#include <memory>
#include <QObject>
#include <optional>

class QDBusServiceWatcher;

namespace QindaQt::Services::NightLight {

struct AutomaticLocationFix final {
    double latitudeDegrees = 0.0;
    double longitudeDegrees = 0.0;
    QDateTime observedAt;
};

enum class AutomaticLocationState { Stopped, Pending, Available, Denied, Unavailable };

// Settings1's automaticLocation choice gates requests; GeoClue remains the
// system authorization authority. Implementations clear fixes on stop, owner
// loss, denial, and stale or malformed responses.
class AutomaticLocationProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~AutomaticLocationProvider() override = default;
    virtual void request() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual AutomaticLocationState state() const = 0;
    [[nodiscard]] virtual std::optional<AutomaticLocationFix> fix() const = 0;
    [[nodiscard]] virtual QString diagnostic() const = 0;

Q_SIGNALS:
    void changed();
};

// Uses GeoClue2 on the injected system bus, pinning every call and signal to
// the resolved unique owner. Late work from a retired request is ignored.
class GeoClueLocationProvider final : public AutomaticLocationProvider {
    Q_OBJECT
public:
    explicit GeoClueLocationProvider(const QDBusConnection &systemBus,
                                     QObject *parent = nullptr);
    ~GeoClueLocationProvider() override;
    void request() override;
    void stop() override;
    [[nodiscard]] AutomaticLocationState state() const override;
    [[nodiscard]] std::optional<AutomaticLocationFix> fix() const override;
    [[nodiscard]] QString diagnostic() const override;

private Q_SLOTS:
    void locationUpdated(const QDBusObjectPath &oldPath, const QDBusObjectPath &newPath);

private:
    void acquireOwner(quint64 generation);
    void createClient(quint64 generation);
    void setProperty(quint64 generation, const QString &property,
                     const QVariant &value, std::function<void()> next);
    void startClient(quint64 generation);
    void readLocation(quint64 generation, const QString &path);
    void fail(quint64 generation, bool denied, const QString &reason);
    void stopRemoteClient();

    QDBusConnection m_bus;
    std::unique_ptr<QDBusServiceWatcher> m_serviceWatcher;
    QString m_owner;
    QString m_clientPath;
    AutomaticLocationState m_state = AutomaticLocationState::Stopped;
    std::optional<AutomaticLocationFix> m_fix;
    QString m_diagnostic;
    quint64 m_generation = 0;
    bool m_signalConnected = false;
    bool m_requested = false;
};

} // namespace QindaQt::Services::NightLight
