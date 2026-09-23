// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/apps/settings_screensaver/lock_screen_saver_store.h>
#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>
#include <qindaqt/session/desktop_controls/screensaver_catalog.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <QtTest>
#include <algorithm>

namespace ScreensaverTestSupport {

using namespace QindaQt::Apps::SettingsScreensaver;
using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

inline constexpr auto kSaverKey = "power.screensaver";
inline constexpr auto kMinutesKey = "power.screensaverMinutes";

// The installed saver set, fixed for the test: two savers the locker's
// wallpaper plugin can draw and one it cannot, which is the split the
// mirror and the status line must be honest about.
class FakeScreensaverCatalog final : public ScreensaverCatalog {
public:
    [[nodiscard]] QList<ScreensaverCatalogEntry> entries() const override
    {
        return {
            {QStringLiteral("qinda-patrol"), QStringLiteral("Qinda Patrol"),
             QStringLiteral("A patrol on every output"), QStringLiteral("qinda-patrol"),
             {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")}, true},
            {QStringLiteral("circuit-reef"), QStringLiteral("Circuit Reef"),
             QStringLiteral("A reef circuit"), QStringLiteral("circuit-reef"),
             {QStringLiteral("--screensaver"), QStringLiteral("--private")}, true},
            {QStringLiteral("prism-brawl"), QStringLiteral("Prism Brawl"),
             QStringLiteral("A brawl of prisms"), QStringLiteral("prism-brawl"),
             {QStringLiteral("--screensaver"), QStringLiteral("--mute")}, false},
        };
    }
};

// The preview, recorded rather than launched: the process boundary has its own
// row (qindaqt.settings-screensaver-preview).
class FakeScreensaverPreview final : public ScreensaverPreview {
public:
    using ScreensaverPreview::ScreensaverPreview;

    [[nodiscard]] Kind kindFor(const QString &token) const override
    {
        if (unavailable) return Kind::Unavailable;
        if (token == ScreensaverPreferences::blankToken()) return Kind::TestingGreeter;
        if (token == QStringLiteral("qinda-patrol")
            || token == QStringLiteral("circuit-reef")) {
            return Kind::TestingGreeter;
        }
        if (token == QStringLiteral("prism-brawl")) return Kind::SaverProgram;
        return Kind::Unavailable;
    }

    [[nodiscard]] bool start(const QString &token, QString *error) override
    {
        // Same refusal as the process boundary: an unavailable kind has
        // nothing to launch.
        if (kindFor(token) == Kind::Unavailable) {
            if (error != nullptr) *error = QStringLiteral("nothing to preview");
            return false;
        }
        if (!startOk) {
            if (error != nullptr) *error = QStringLiteral("preview refused");
            return false;
        }
        ++starts;
        lastToken = token;
        return true;
    }

    [[nodiscard]] bool running() const override { return false; }

    bool unavailable = false;
    bool startOk = true;
    int starts = 0;
    QString lastToken;
};

// The greeter mirror, recorded rather than written: a route test must never
// depend on a real kscreenlockerrc, and the mirror's own file format has its
// own row (qindaqt.settings-screensaver-lock-screen-saver-store).
class FakeLockScreenSaverStore final : public LockScreenSaverStore {
public:
    [[nodiscard]] bool save(const QString &saverToken, QString *error) override
    {
        ++m_saves;
        if (m_failure.isEmpty()) {
            m_current = saverToken;
            if (error != nullptr) error->clear();
            return true;
        }
        if (error != nullptr) *error = m_failure;
        return false;
    }

    [[nodiscard]] QString currentSaver() const override { return m_current; }

    void failWith(const QString &message) { m_failure = message; }
    [[nodiscard]] int saves() const noexcept { return m_saves; }

private:
    QString m_current = QStringLiteral("none");
    QString m_failure;
    int m_saves = 0;
};

// AGENT-NOTE: the screensaver route owns a two-key scope, so this fake keeps a
// value map rather than the single value the idle display fake carries. An
// applied commit writes its own operation back into that map, which is how the
// route's "only a confirmed snapshot reconciles the rows" contract is proven.
class FakeSettingsTransport final : public SettingsTransport {
public:
    bool start(QString *error) override
    {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    void stop() override {}

    void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override
    {
        if (!autoSnapshots) {
            m_snapshotRequests.append({token, owner});
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, token, owner] {
                Q_EMIT snapshotReceived(token, owner, snapshotWire(++m_revision));
            },
            Qt::QueuedConnection);
    }

    void replyToLastSnapshot(quint64 revision = 0)
    {
        QVERIFY(!m_snapshotRequests.isEmpty());
        const auto request = m_snapshotRequests.takeLast();
        if (revision == 0) revision = ++m_revision;
        else m_revision = std::max(m_revision, revision);
        Q_EMIT snapshotReceived(request.first, request.second, snapshotWire(revision));
    }

    [[nodiscard]] int pendingSnapshotCount() const noexcept
    {
        return int(m_snapshotRequests.size());
    }

    bool autoSnapshots = true;

    struct CommitRequest {
        quint64 token = 0;
        QString owner;
        QString epoch;
        quint64 baseRevision = 0;
    };

    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 baseRevision, const QVariantList &operations) override
    {
        for (const QVariant &operation : operations) {
            m_committedOperations.append(operation.toMap());
        }
        m_commits.append(CommitRequest{token, owner, epoch, baseRevision});
    }

    void requestActivation() override {}

    // Answers the pending commit synchronously: the model sees exactly one
    // deterministic reply per write, and an applied one becomes new truth.
    void replyToLastCommit(SettingsWireStatus status)
    {
        QVERIFY(!m_commits.isEmpty());
        QVERIFY(!m_committedOperations.isEmpty());
        const CommitRequest request = m_commits.takeLast();
        const QVariantMap operation = m_committedOperations.constLast();
        m_revision = request.baseRevision + 1;
        const quint64 after =
            status == SettingsWireStatus::Applied || status == SettingsWireStatus::Conflict
                ? m_revision : request.baseRevision;
        m_commitStatus = status;
        QStringList changed;
        if (status == SettingsWireStatus::Applied) {
            const QString key = operation.value(QStringLiteral("key")).toString();
            m_values.insert(key, operation.value(QStringLiteral("value")));
            changed.append(key);
        }
        Q_EMIT commitReceived(request.token, request.owner,
                              commitWire(status == SettingsWireStatus::Conflict ? after
                                                                         : request.baseRevision,
                                         after, changed));
    }

    [[nodiscard]] int pendingCommitCount() const noexcept
    {
        return static_cast<int>(m_commits.size());
    }

    void announceOwner(const QString &owner = QStringLiteral(":1.11"))
    {
        QMetaObject::invokeMethod(
            this, [this, owner] { Q_EMIT ownerChanged(owner); },
            Qt::QueuedConnection);
    }

    void failLastCommit()
    {
        QVERIFY(!m_commits.isEmpty());
        const CommitRequest request = m_commits.takeLast();
        Q_EMIT requestFailed(request.token, request.owner,
                             QStringLiteral("org.test.Timeout"),
                             QStringLiteral("commit reply was lost"));
    }

    void setValue(const char *key, const QVariant &value)
    {
        m_values.insert(QString::fromLatin1(key), value);
    }

    [[nodiscard]] const QList<QVariantMap> &committedOperations() const noexcept
    {
        return m_committedOperations;
    }

    [[nodiscard]] QVariantMap snapshotWire(quint64 revision) const
    {
        return {{QLatin1StringView(WireContract::FieldStatus),
                 quint32(SettingsWireStatus::Applied)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), m_epoch},
                {QLatin1StringView(WireContract::FieldRevision), revision},
                {QLatin1StringView(WireContract::FieldValues), m_values},
                {QLatin1StringView(WireContract::FieldSourceLayers), sourceLayers()},
                {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

private:
    [[nodiscard]] QVariantMap sourceLayers() const
    {
        QVariantMap layers;
        for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
            layers.insert(it.key(), QStringLiteral("user-overrides"));
        }
        return layers;
    }

    [[nodiscard]] QVariantMap commitWire(quint64 before, quint64 after,
                                         const QStringList &changed) const
    {
        const QString key = m_committedOperations.constLast()
                                .value(QStringLiteral("key")).toString();
        const QVariantMap values{{key, m_values.value(key)}};
        const QVariantMap layers{{key, QStringLiteral("user-overrides")}};
        return {{QLatin1StringView(WireContract::FieldStatus), quint32(m_commitStatus)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), m_epoch},
                {QLatin1StringView(WireContract::FieldRevisionBefore), before},
                {QLatin1StringView(WireContract::FieldRevisionAfter), after},
                {QLatin1StringView(WireContract::FieldValues), values},
                {QLatin1StringView(WireContract::FieldSourceLayers), layers},
                {QLatin1StringView(WireContract::FieldChangedKeys), changed},
                {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

    QString m_epoch = QStringLiteral("epoch-11");
    quint64 m_revision = 0;
    QVariantMap m_values{{QString::fromLatin1(kSaverKey), QStringLiteral("none")},
                         {QString::fromLatin1(kMinutesKey),
                          QVariant::fromValue<qint64>(5)}};
    SettingsWireStatus m_commitStatus = SettingsWireStatus::Applied;
    QList<QVariantMap> m_committedOperations;
    QList<CommitRequest> m_commits;
    QList<QPair<quint64, QString>> m_snapshotRequests;
};

} // namespace ScreensaverTestSupport
