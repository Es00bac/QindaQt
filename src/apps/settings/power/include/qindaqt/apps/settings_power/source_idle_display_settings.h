// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace QindaQt::Apps::SettingsPower {

// Settings view for the effective per-source idle display preferences. Values
// are read from one exact-owner Settings1 scope; writes are never replayed and
// remain pending until a newer confirmed snapshot reads back the exact value.
class SourceIdleDisplaySettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sourceRows READ sourceRows NOTIFY changed)
    Q_PROPERTY(QString activeSource READ activeSource NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
public:
    SourceIdleDisplaySettingsModel(Power::PowerClient &power,
                                   Services::SettingsClient::SettingsClient &settings,
                                   QObject *parent = nullptr);
    [[nodiscard]] QVariantList sourceRows() const;
    [[nodiscard]] QString activeSource() const;
    [[nodiscard]] bool available() const noexcept { return m_available; }
    [[nodiscard]] bool busy() const noexcept { return m_pending; }
    [[nodiscard]] const QString &statusText() const noexcept { return m_statusText; }
    [[nodiscard]] const QString &errorText() const noexcept { return m_errorText; }
    Q_INVOKABLE bool setEnabled(const QString &source, bool enabled);
    Q_INVOKABLE bool setSeconds(const QString &source, int seconds);
    Q_INVOKABLE bool retry();

Q_SIGNALS:
    void changed();

private:
    struct Values {
        bool confirmed = false;
        bool enabled = false;
        int seconds = 0;
        QString enabledSource;
        QString secondsSource;
    };
    void updatePowerSource();
    void applySnapshot(bool fresh);
    void handleClientState();
    void handleCommit(const Services::SettingsClient::CommitOutcome &outcome);
    void markUncertain(const QString &reason);
    void publish();
    bool submit(const QString &key, const QVariant &value);
    static QString key(const QString &source, const QString &field);
    static bool validSource(const QString &source);

    Power::PowerClient &m_power;
    Services::SettingsClient::SettingsClient &m_settings;
    QTimer m_readbackTimeout;
    QHash<QString, Values> m_values;
    QString m_activeSource;
    QString m_statusText;
    QString m_errorText;
    QString m_pendingKey;
    QString m_pendingOwner;
    QString m_pendingEpoch;
    QVariant m_requestedValue;
    quint64 m_revisionFloor = 0;
    quint64 m_snapshotSequence = 0;
    quint64 m_snapshotSequenceAtDispatch = 0;
    quint64 m_requiredSnapshotSequence = 0;
    bool m_available = false;
    bool m_pending = false;
    bool m_waitingForReadback = false;
};

} // namespace QindaQt::Apps::SettingsPower
