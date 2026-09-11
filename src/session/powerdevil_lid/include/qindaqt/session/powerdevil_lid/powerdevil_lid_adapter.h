// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtDBus/QDBusConnection>

class QDBusServiceWatcher;

namespace QindaQt::Session::PowerDevilLid {

// Narrow GUI-thread adapter for PowerDevil's lid-close and power-button
// profile policy (ADR-0132). It borrows the injected session bus and writes
// only LidAction, InhibitLidActionWhenExternalMonitorPresent, and
// PowerButtonAction in the SuspendAndShutdown group of the AC, Battery, and
// LowBattery profiles of powerdevilrc. PowerDevil remains the sole owner of
// button and lid interpretation; this type only persists preferences and asks
// the running daemon to reload them.
// AGENT-CONTRACT: the caller owns the injected bus connection and keeps this
// GUI-thread object alive until applyFinished; errors are reported through the
// error property and applyFinished, and a synced config is not rolled back if
// a subsequent daemon refresh fails.
class PowerDevilLidAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY applyingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(quint32 lidAction READ lidAction NOTIFY preferencesChanged)
    Q_PROPERTY(bool inhibitLidActionWhenExternalMonitorPresent READ
                   inhibitLidActionWhenExternalMonitorPresent
                   NOTIFY preferencesChanged)
    Q_PROPERTY(quint32 powerButtonAction READ powerButtonAction NOTIFY
                   preferencesChanged)

public:
    // AGENT-CONTRACT: these numbers are PowerDevil::PowerButtonAction from
    // upstream powerdevil daemon/powerdevilenums.h at tag v6.6.6 (installed as
    // libpowerdevilcore.so.6.6.6); PowerDevil casts both LidAction and
    // PowerButtonAction config keys to that enum. Never invent a value: add a
    // supported one only after rechecking the cited header.
    static constexpr quint32 DoNothing = 0;
    static constexpr quint32 Sleep = 1;
    static constexpr quint32 Hibernate = 2;
    static constexpr quint32 ShutDown = 8;
    static constexpr quint32 LockScreen = 32;
    static constexpr quint32 TurnOffScreen = 64;

    explicit PowerDevilLidAdapter(QDBusConnection sessionBus,
                                  QObject *parent = nullptr);
    ~PowerDevilLidAdapter() override;

    void start();
    void stop();

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool applying() const noexcept;
    [[nodiscard]] const QString &error() const noexcept;
    // Current profile truth for display. Values are read from powerdevilrc
    // when the owner becomes available; an absent entry reads as the
    // documented zero value (NoAction) and true for the inhibit flag.
    [[nodiscard]] quint32 lidAction() const noexcept;
    [[nodiscard]] bool inhibitLidActionWhenExternalMonitorPresent() const noexcept;
    [[nodiscard]] quint32 powerButtonAction() const noexcept;

    // True only for the six supported PowerButtonAction values above.
    [[nodiscard]] static bool isSupportedAction(quint32 action);

    // Persist the requested values and ask PowerDevil to reparse and reload
    // its active profile. A false return means nothing was written.
    Q_INVOKABLE bool apply(quint32 lidAction,
                           bool inhibitLidActionWhenExternalMonitorPresent,
                           quint32 powerButtonAction);

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
    [[nodiscard]] bool writePreferences(quint32 lidAction,
                                        bool inhibitLidActionWhenExternalMonitorPresent,
                                        quint32 powerButtonAction,
                                        QString *error) const;

    QDBusConnection m_sessionBus;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    bool m_available = false;
    bool m_applying = false;
    quint32 m_lidAction = 0;
    bool m_inhibitLidActionWhenExternalMonitorPresent = true;
    quint32 m_powerButtonAction = 0;
    quint64 m_requestSerial = 0;
    QString m_error;
};

} // namespace QindaQt::Session::PowerDevilLid
