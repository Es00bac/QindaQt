// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtDBus/QDBusConnection>

class QDBusServiceWatcher;

namespace QindaQt::Session::PowerDevilProfile {

// Narrow GUI-thread adapter for PowerDevil's per-power-source automatic
// power-profiles-daemon switch (Checkpoint L row 5's remaining piece).
// PowerDevil's own "Performance" bundled action reads
// `[<AC|Battery|LowBattery>][Performance] PowerProfile=<ppd-profile-id>`
// from powerdevilrc and applies it autonomously when the power source
// changes -- the exact same per-source-group mechanism ADR-0105 already
// verified for `[<source>][Display] TurnOffDisplayIdleTimeoutSec`. This
// adapter only persists the three preferences and asks the running daemon
// to reparse and reload; it never switches a profile itself.
//
// AGENT-CONTRACT: unlike lid/power-button actions (ADR-0132), a PPD profile
// id is not a fixed enum: it is whatever power-profiles-daemon currently
// reports (typically "power-saver"/"balanced"/"performance", but not
// guaranteed). This adapter does not validate ids against that list --
// the caller (the route model, which already holds the live Power1
// profile list) is responsible for offering only ids PowerDevil/ppd will
// actually accept. An empty id for a source means "no automatic switch for
// that source": the corresponding `PowerProfile` key is deleted rather than
// written empty, since PowerDevil's own KConfigSkeleton has no documented
// meaning for an empty value.
class PowerDevilProfileAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY applyingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString acProfileId READ acProfileId NOTIFY preferencesChanged)
    Q_PROPERTY(QString batteryProfileId READ batteryProfileId NOTIFY
                   preferencesChanged)
    Q_PROPERTY(QString lowBatteryProfileId READ lowBatteryProfileId NOTIFY
                   preferencesChanged)

public:
    explicit PowerDevilProfileAdapter(QDBusConnection sessionBus,
                                      QObject *parent = nullptr);
    ~PowerDevilProfileAdapter() override;

    void start();
    void stop();

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool applying() const noexcept;
    [[nodiscard]] const QString &error() const noexcept;
    [[nodiscard]] const QString &acProfileId() const noexcept;
    [[nodiscard]] const QString &batteryProfileId() const noexcept;
    [[nodiscard]] const QString &lowBatteryProfileId() const noexcept;

    // Persist the requested per-source ids (empty = no automatic switch for
    // that source) and ask PowerDevil to reparse and reload. A false return
    // means nothing was written.
    Q_INVOKABLE bool apply(const QString &acProfileId,
                           const QString &batteryProfileId,
                           const QString &lowBatteryProfileId);

Q_SIGNALS:
    void availabilityChanged();
    void applyingChanged();
    void errorChanged();
    void preferencesChanged();
    void applyFinished(bool success, const QString &error);

private:
    void refreshAvailability();
    void readPreferences();

private Q_SLOTS:
    void ownerChanged(const QString &serviceName, const QString &oldOwner,
                      const QString &newOwner);

private:
    void requestRefresh(bool recovery);
    void finishApply(bool success, const QString &error);
    void setAvailable(bool available);
    void setError(const QString &error);
    [[nodiscard]] bool writePreferences(const QString &acProfileId,
                                        const QString &batteryProfileId,
                                        const QString &lowBatteryProfileId,
                                        QString *error) const;

    QDBusConnection m_sessionBus;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    bool m_available = false;
    bool m_applying = false;
    QString m_acProfileId;
    QString m_batteryProfileId;
    QString m_lowBatteryProfileId;
    quint64 m_requestSerial = 0;
    QString m_error;
};

} // namespace QindaQt::Session::PowerDevilProfile
