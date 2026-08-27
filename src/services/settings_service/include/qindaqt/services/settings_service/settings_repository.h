// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/settings/layered_settings.h"
#include "qindaqt/settings/settings_schema.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

namespace QindaQt::Services::SettingsService {

enum class RepositoryCommitStatus {
    Applied,
    ValidationFailed,
    Conflict,
    ReadOnlyLayer,
    PersistenceFailed,
    RevisionExhausted,
    UnknownKey,
};

struct RepositoryCommitResult final {
    RepositoryCommitStatus status = RepositoryCommitStatus::ValidationFailed;
    quint64 revisionBefore = 0;
    quint64 revisionAfter = 0;
    // Authoritative effective value/source for every key named by the
    // request's operations, reflecting whatever the model actually holds
    // *after* this call returns -- on every outcome except UnknownKey. That
    // status has exactly empty maps because at least one requested key has no
    // authority and a partial map would be ambiguous. A conflict/failure
    // caller can otherwise compare this against what it requested to decide
    // whether to settle, retry, or surface an alert.
    QVariantMap currentValues;
    QMap<QString, Settings::SettingLayer> currentSourceLayers;
    // Non-empty only for a real (non-no-op) Applied commit.
    QStringList changedKeys;
    QString message;

    [[nodiscard]] bool ok() const noexcept { return status == RepositoryCommitStatus::Applied; }
};

struct RepositorySnapshot final {
    bool ok = false;
    QVariantMap values;
    QMap<QString, Settings::SettingLayer> sourceLayers;
    quint64 revision = 0;
    // Set only when ok is false because a requested key is not defined by
    // the active schema.
    QString unknownKey;
};

// AGENT-CONTRACT: owns the one authoritative in-memory LayeredSettings
// instance and the on-disk UserOverrides document for this service process.
// Every mutating call is copy-on-write: a candidate clone is built, the
// transaction is validated and applied to the *candidate*, and -- only if
// that produced a real (non-no-op) change -- the candidate's UserOverrides
// layer is saved to disk. Only after a successful save (or for a true
// no-op, which never touches disk) does the authoritative model swap to the
// candidate and the revision advance. A failure at any step leaves the
// authoritative model, the on-disk document, and the revision exactly as
// they were before the call; nothing is ever partially applied. Not
// thread-safe -- the owning D-Bus object must call it only from its
// connection's thread.
class SettingsRepository final {
public:
    struct Operation final {
        QString key;
        bool remove = false;
        QVariant value;
    };

    SettingsRepository(Settings::LayeredSettings initial, QString userOverridesPath, QString epoch,
                       quint64 initialRevision = 0);

    [[nodiscard]] const QString &epoch() const noexcept { return m_epoch; }
    [[nodiscard]] quint64 revision() const noexcept { return m_revision; }
    [[nodiscard]] const Settings::SettingsSchema &schema() const noexcept { return m_settings.schema(); }

    // Rejects the whole request (ok=false, unknownKey set) rather than
    // silently returning a partial map when any requested key is not
    // defined by the active schema.
    [[nodiscard]] RepositorySnapshot snapshot(const QStringList &keys) const;

    [[nodiscard]] RepositoryCommitResult commitUserOverrides(quint64 baseRevision,
                                                             const QVector<Operation> &operations);

    // Pure boundary check extracted so the revision-exhaustion guard is
    // testable without driving a real model through 2^64 commits.
    [[nodiscard]] static bool wouldExhaustRevision(quint64 currentRevision) noexcept;

private:
    [[nodiscard]] RepositoryCommitResult currentAsResult(RepositoryCommitStatus status, quint64 revisionBefore,
                                                         const QVector<Operation> &operations,
                                                         QString message) const;
    [[nodiscard]] bool persistCandidate(const Settings::LayeredSettings &candidate, QString *error) const;

    Settings::LayeredSettings m_settings;
    QString m_userOverridesPath;
    QString m_epoch;
    // Service lineage revision is deliberately independent of the settings
    // model's internal transaction counter. The service owns the process-local
    // nonwrapping wire counter and advances it only after durable publication.
    quint64 m_revision = 0;
};

} // namespace QindaQt::Services::SettingsService
