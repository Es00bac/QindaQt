// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/night_light/night_light_schedule.h>

#include <QDBusConnection>
#include <QObject>
#include <optional>

namespace QindaQt::Services::NightLight {

struct NightLightScheduleState final {
    NightLightSettings settings;
    ScheduleFrame schedule;
    QStringList disabledOutputUuids;
    bool outputIdentityAvailable = true;
    quint64 revision = 0;
    friend bool operator==(const NightLightScheduleState &,
                           const NightLightScheduleState &) = default;
};

// Owns one exact schedule-service owner and subscription nonce. Frames are
// accepted only from that unique sender with the current nonce/cookie and a
// strictly increasing revision; method replies never authorize live state.
class QtNightLightScheduleClient final : public QObject {
    Q_OBJECT
public:
    explicit QtNightLightScheduleClient(const QDBusConnection &connection,
                                        QObject *parent = nullptr);
    ~QtNightLightScheduleClient() override;
    void start();
    void stop();
    [[nodiscard]] const std::optional<NightLightScheduleState> &state() const noexcept;

Q_SIGNALS:
    void stateChanged(const QindaQt::Services::NightLight::NightLightScheduleState &state);
    void unavailable(const QString &reason);

private Q_SLOTS:
    void receiveFrame(const QByteArray &nonce, qulonglong cookie,
                      qulonglong revision, const QVariantMap &values);

private:
    void resolveOwner();
    void bindOwner(const QString &owner);
    void subscribe();
    void clearState(QString reason);
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Services::NightLight

Q_DECLARE_METATYPE(QindaQt::Services::NightLight::NightLightScheduleState)
