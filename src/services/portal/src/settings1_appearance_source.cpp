// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/portal/settings1_appearance_source.h"

#include "qindaqt/services/portal/appearance_policy.h"
#include "qindaqt/services/settings_client/settings_client.h"

#include <utility>

namespace QindaQt::Services::Portal {

using QindaQt::Services::SettingsClient::ClientState;

Settings1AppearanceSource::Settings1AppearanceSource(
    QindaQt::Services::SettingsClient::SettingsClient &client,
    const AppearancePolicyProjector &projector,
    QObject *parent)
    : AppearanceSource(parent)
    , m_client(client)
    , m_projector(projector)
{
    connect(&m_client,
            &QindaQt::Services::SettingsClient::SettingsClient::stateChanged,
            this, &Settings1AppearanceSource::synchronize);
    connect(&m_client,
            &QindaQt::Services::SettingsClient::SettingsClient::snapshotChanged,
            this, &Settings1AppearanceSource::synchronize);
}

bool Settings1AppearanceSource::start(QString *error)
{
    if (m_started) {
        return true;
    }
    if (!m_projector.isValid()) {
        if (error != nullptr) {
            *error = m_projector.catalogError();
        }
        return false;
    }
    m_started = true;
    if (!m_client.start(error)) {
        m_started = false;
        return false;
    }
    synchronize();
    return true;
}

void Settings1AppearanceSource::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;
    m_client.stop();
    const bool changed = m_current.has_value() || !m_diagnostic.isEmpty();
    m_current.reset();
    m_diagnostic.clear();
    if (changed) {
        Q_EMIT currentChanged();
    }
}

const std::optional<AppearanceTruth> &Settings1AppearanceSource::current() const
{
    return m_current;
}

QString Settings1AppearanceSource::diagnostic() const
{
    return m_diagnostic;
}

void Settings1AppearanceSource::synchronize()
{
    std::optional<AppearanceTruth> next;
    QString diagnostic;
    if (!m_started) {
        diagnostic = QStringLiteral("appearance Settings1 source is stopped");
    } else if (m_client.state() != ClientState::Ready
               || !m_client.snapshot().has_value()) {
        diagnostic = m_client.lastError().isEmpty()
            ? QStringLiteral("appearance Settings1 authority is unavailable")
            : m_client.lastError();
    } else {
        const auto &snapshot = *m_client.snapshot();
        const AppearanceProjectionResult projected =
            m_projector.project(snapshot.values);
        if (!projected.ok()) {
            diagnostic = projected.diagnostic;
        } else if (snapshot.owner.isEmpty() || snapshot.epoch.isEmpty()) {
            diagnostic = QStringLiteral("appearance Settings1 lineage is empty");
        } else {
            next = AppearanceTruth{.policy = *projected.policy,
                                   .owner = snapshot.owner,
                                   .epoch = snapshot.epoch,
                                   .revision = snapshot.revision};
        }
    }
    if (next == m_current && diagnostic == m_diagnostic) {
        return;
    }
    m_current = std::move(next);
    m_diagnostic = std::move(diagnostic);
    Q_EMIT currentChanged();
}

} // namespace QindaQt::Services::Portal
