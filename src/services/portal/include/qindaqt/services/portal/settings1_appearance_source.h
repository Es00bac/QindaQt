// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/portal/appearance_source.h"

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::Services::Portal {

class AppearancePolicyProjector;

// Exact-owner Settings1 consumer. SettingsClient owns activation, bounded
// subscription, owner/epoch validation, debounce, timeout, and stale-reply
// fencing; this class adds atomic QST projection and immediately withdraws
// current truth whenever the client leaves Ready.
class Settings1AppearanceSource final : public AppearanceSource {
    Q_OBJECT
public:
    Settings1AppearanceSource(
        QindaQt::Services::SettingsClient::SettingsClient &client,
        const AppearancePolicyProjector &projector,
        QObject *parent = nullptr);

    [[nodiscard]] bool start(QString *error = nullptr) override;
    void stop() override;
    [[nodiscard]] const std::optional<AppearanceTruth> &current() const override;
    [[nodiscard]] QString diagnostic() const override;

private:
    void synchronize();

    QindaQt::Services::SettingsClient::SettingsClient &m_client;
    const AppearancePolicyProjector &m_projector;
    std::optional<AppearanceTruth> m_current;
    QString m_diagnostic;
    bool m_started = false;
};

} // namespace QindaQt::Services::Portal
