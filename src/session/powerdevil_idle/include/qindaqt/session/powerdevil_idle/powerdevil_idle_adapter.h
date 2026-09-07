// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtDBus/QDBusConnection>

class QDBusServiceWatcher;

namespace QindaQt::Session::PowerDevilIdle {

// Small GUI-thread adapter for PowerDevil's documented profile configuration.
// It borrows the injected session bus and writes only the Display idle-off
// entries in AC, Battery, and LowBattery profiles. PowerDevil remains the sole
// owner of idle timers and DPMS; this type only persists preferences and asks
// the running daemon to reload them.
// AGENT-CONTRACT: the caller owns the injected bus connection and keeps this
// GUI-thread object alive until applyFinished; errors are reported through the
// error property and applyFinished, and a synced config is not rolled back if
// a subsequent daemon refresh fails.
class PowerDevilIdleAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(bool enabled READ enabled NOTIFY preferencesChanged)
    Q_PROPERTY(int minutes READ minutes NOTIFY preferencesChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY applyingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    static constexpr int MinimumMinutes = 1;
    static constexpr int MaximumMinutes = 9'999;

    explicit PowerDevilIdleAdapter(QDBusConnection sessionBus,
                                   QObject *parent = nullptr);
    ~PowerDevilIdleAdapter() override;

    void start();
    void stop();

    [[nodiscard]] bool available() const noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    [[nodiscard]] int minutes() const noexcept;
    [[nodiscard]] bool applying() const noexcept;
    [[nodiscard]] const QString &error() const noexcept;

    // Persist the requested values and ask PowerDevil to reparse and reload
    // its active profile. A false return means no preference was written.
    Q_INVOKABLE bool apply(bool enabled, int minutes);

Q_SIGNALS:
    void availabilityChanged();
    void preferencesChanged();
    void applyingChanged();
    void errorChanged();
    void applyFinished(bool success, const QString &error);

private:
    void refreshAvailability();

private Q_SLOTS:
    void ownerChanged(const QString &serviceName, const QString &oldOwner,
                      const QString &newOwner);

private:
    void requestRefresh(bool recovery);
    void finishApply(bool success, const QString &error);
    void setAvailable(bool available);
    void setError(const QString &error);
    [[nodiscard]] bool writePreferences(bool enabled, int minutes,
                                         QString *error) const;

    QDBusConnection m_sessionBus;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
    bool m_available = false;
    bool m_enabled = false;
    int m_minutes = 1;
    bool m_applying = false;
    quint64 m_requestSerial = 0;
    QString m_error;
};

} // namespace QindaQt::Session::PowerDevilIdle
