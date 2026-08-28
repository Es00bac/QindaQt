// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_settings_model.h"

namespace QindaQt::Apps::SettingsAppearance {
constexpr int MaximumResultMessageLength = 512;

QString AppearanceSettingsModel::saveResultStateToken(SaveResultState state)
{
    switch (state) {
    case AppearanceSettingsModel::SaveResultState::NotAttempted:
        return QStringLiteral("not-attempted");
    case AppearanceSettingsModel::SaveResultState::Applied:
        return QStringLiteral("applied");
    case AppearanceSettingsModel::SaveResultState::Conflict:
        return QStringLiteral("conflict");
    case AppearanceSettingsModel::SaveResultState::Failed:
        return QStringLiteral("failed");
    case AppearanceSettingsModel::SaveResultState::Uncertain:
        return QStringLiteral("uncertain");
    }
    return QStringLiteral("failed");
}

QString AppearanceSettingsModel::saveResultStateLabel(SaveResultState state)
{
    switch (state) {
    case AppearanceSettingsModel::SaveResultState::NotAttempted:
        return QStringLiteral("Not attempted");
    case AppearanceSettingsModel::SaveResultState::Applied:
        return QStringLiteral("Applied");
    case AppearanceSettingsModel::SaveResultState::Conflict:
        return QStringLiteral("Conflict");
    case AppearanceSettingsModel::SaveResultState::Failed:
        return QStringLiteral("Failed");
    case AppearanceSettingsModel::SaveResultState::Uncertain:
        return QStringLiteral("Outcome uncertain");
    }
    return QStringLiteral("Failed");
}

QVariantList AppearanceSettingsModel::saveResults() const
{
    QVariantList results;
    results.reserve(m_saveResults.size());
    for (const SaveResult &result : m_saveResults) {
        results.append(QVariantMap{
            {QStringLiteral("key"), result.key},
            {QStringLiteral("state"), saveResultStateToken(result.state)},
            {QStringLiteral("message"), result.message},
        });
    }
    return results;
}

QString AppearanceSettingsModel::saveResultsText() const
{
    if (m_saveResults.isEmpty()) {
        return {};
    }
    QStringList entries;
    entries.reserve(m_saveResults.size());
    for (const SaveResult &result : m_saveResults) {
        QString entry = QStringLiteral("%1 — %2")
                            .arg(result.key, saveResultStateLabel(result.state));
        if (!result.message.isEmpty()) {
            entry += QStringLiteral(": %1").arg(result.message);
        }
        entries.append(entry);
    }
    return QStringLiteral("Save results: %1").arg(entries.join(QStringLiteral("; ")));
}

bool AppearanceSettingsModel::saveResultsHaveFailure() const noexcept
{
    for (const SaveResult &result : m_saveResults) {
        if (result.state == SaveResultState::Conflict
            || result.state == SaveResultState::Failed
            || result.state == SaveResultState::Uncertain) {
            return true;
        }
    }
    return false;
}

void AppearanceSettingsModel::beginSaveResults()
{
    m_saveResults.clear();
    m_saveResults.reserve(m_queue.size());
    for (const CommitIntent &intent : m_queue) {
        m_saveResults.append(
            SaveResult{intent.key, SaveResultState::NotAttempted, {}});
    }
    Q_EMIT saveResultsChanged();
}

void AppearanceSettingsModel::updateSaveResult(const QString &key,
                                               SaveResultState state,
                                               QString message)
{
    for (SaveResult &result : m_saveResults) {
        if (result.key != key) {
            continue;
        }
        result.state = state;
        result.message = message.left(MaximumResultMessageLength);
        Q_EMIT saveResultsChanged();
        return;
    }
}

void AppearanceSettingsModel::clearSaveResults()
{
    if (m_saveResults.isEmpty()) {
        return;
    }
    m_saveResults.clear();
    Q_EMIT saveResultsChanged();
}

} // namespace QindaQt::Apps::SettingsAppearance
