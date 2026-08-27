// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/settings_service/settings_repository.h"

#include "qindaqt/settings/settings_document.h"

#include <limits>
#include <utility>

namespace QindaQt::Services::SettingsService {

using QindaQt::Settings::CommitStatus;
using QindaQt::Settings::LayeredSettings;
using QindaQt::Settings::SettingLayer;
using QindaQt::Settings::SettingsDocument;
using QindaQt::Settings::SettingsFileStore;

SettingsRepository::SettingsRepository(LayeredSettings initial, QString userOverridesPath, QString epoch,
                                       quint64 initialRevision)
    : m_settings(std::move(initial))
    , m_userOverridesPath(std::move(userOverridesPath))
    , m_epoch(std::move(epoch))
    , m_revision(initialRevision)
{
}

bool SettingsRepository::wouldExhaustRevision(quint64 currentRevision) noexcept
{
    return currentRevision == std::numeric_limits<quint64>::max();
}

RepositorySnapshot SettingsRepository::snapshot(const QStringList &keys) const
{
    RepositorySnapshot result;
    for (const auto &key : keys) {
        if (!m_settings.schema().contains(key)) {
            result.ok = false;
            result.unknownKey = key;
            return result;
        }
    }
    for (const auto &key : keys) {
        result.values.insert(key, m_settings.value(key));
        if (const auto source = m_settings.sourceLayer(key)) {
            result.sourceLayers.insert(key, *source);
        }
    }
    result.revision = m_revision;
    result.ok = true;
    return result;
}

RepositoryCommitResult SettingsRepository::currentAsResult(RepositoryCommitStatus status, quint64 revisionBefore,
                                                            const QVector<Operation> &operations,
                                                            QString message) const
{
    RepositoryCommitResult result;
    result.status = status;
    result.revisionBefore = revisionBefore;
    result.revisionAfter = m_revision;
    for (const auto &operation : operations) {
        // AGENT-GUARD: an absent schema key has no authoritative QVariant or
        // source. Skipping it here prevents future error paths from turning
        // absence into invalid-QVariant JSON null and corrupting wire status.
        if (!m_settings.schema().contains(operation.key)) {
            continue;
        }
        result.currentValues.insert(operation.key, m_settings.value(operation.key));
        if (const auto source = m_settings.sourceLayer(operation.key)) {
            result.currentSourceLayers.insert(operation.key, *source);
        }
    }
    result.message = std::move(message);
    return result;
}

bool SettingsRepository::persistCandidate(const LayeredSettings &candidate, QString *error) const
{
    SettingsDocument document;
    document.schemaVersion = candidate.schema().version();
    document.layer = SettingLayer::UserOverrides;
    document.values = candidate.layerValues(SettingLayer::UserOverrides);
    return SettingsFileStore::save(m_userOverridesPath, document, candidate.schema(), nullptr, error);
}

RepositoryCommitResult SettingsRepository::commitUserOverrides(quint64 baseRevision,
                                                                const QVector<Operation> &operations)
{
    const quint64 revisionBefore = m_revision;
    for (const auto &operation : operations) {
        if (!m_settings.schema().contains(operation.key)) {
            // AGENT-CONTRACT: after service epoch/envelope fencing, UnknownKey
            // precedes base-revision and exhaustion checks. Its maps are
            // exactly empty: partial authority for a mixed known/unknown
            // transaction would not describe one atomic result.
            RepositoryCommitResult result;
            result.status = RepositoryCommitStatus::UnknownKey;
            result.revisionBefore = revisionBefore;
            result.revisionAfter = revisionBefore;
            result.message = QStringLiteral("unknown key: %1").arg(operation.key);
            return result;
        }
    }
    if (baseRevision != revisionBefore) {
        return currentAsResult(RepositoryCommitStatus::Conflict, revisionBefore, operations,
                               QStringLiteral("stale base revision"));
    }
    if (wouldExhaustRevision(revisionBefore)) {
        return currentAsResult(RepositoryCommitStatus::RevisionExhausted, revisionBefore, operations,
                               QStringLiteral("revision counter is exhausted"));
    }

    // Copy-on-write candidate: every mutation below happens on this clone.
    // The authoritative m_settings is not touched until persistence (or a
    // true no-op that needs none) succeeds.
    LayeredSettings candidate = m_settings;
    auto transaction = candidate.beginTransaction(SettingLayer::UserOverrides);
    for (const auto &operation : operations) {
        if (operation.remove) {
            transaction.removeValue(operation.key);
        } else {
            transaction.setValue(operation.key, operation.value);
        }
    }

    const auto committed = candidate.commit(transaction);
    if (!committed.ok()) {
        RepositoryCommitStatus status = RepositoryCommitStatus::ValidationFailed;
        if (committed.status == CommitStatus::Conflict) {
            status = RepositoryCommitStatus::Conflict;
        } else if (committed.status == CommitStatus::ReadOnlyLayer) {
            status = RepositoryCommitStatus::ReadOnlyLayer;
        }
        const QString message = committed.validation.isValid() ? QStringLiteral("commit rejected")
                                                                : committed.validation.summary();
        return currentAsResult(status, revisionBefore, operations, message);
    }

    if (committed.changes.isEmpty()) {
        // A semantic no-op: nothing to persist, nothing to swap, nothing to
        // publish. The authoritative model already reflects this state.
        return currentAsResult(RepositoryCommitStatus::Applied, revisionBefore, operations, {});
    }

    QString persistError;
    if (!persistCandidate(candidate, &persistError)) {
        // Nothing was swapped: memory, disk, and revision remain exactly as
        // they were before this call.
        return currentAsResult(RepositoryCommitStatus::PersistenceFailed, revisionBefore, operations,
                               persistError);
    }

    m_settings = std::move(candidate);
    ++m_revision;
    RepositoryCommitResult result = currentAsResult(RepositoryCommitStatus::Applied, revisionBefore, operations, {});
    result.changedKeys = committed.changes.touchedKeys;
    return result;
}

} // namespace QindaQt::Services::SettingsService
