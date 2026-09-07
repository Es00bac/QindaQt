// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_power/idle_display_settings.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/session/desktop_controls/settings1_idle_preferences.h>

#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsPower;
using namespace QindaQt::Session::DesktopControls;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

constexpr auto kIdleKey = "power.idleDisplayOffMinutes";

class FakeSettingsTransport final : public SettingsTransport {
    Q_OBJECT

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
        QMetaObject::invokeMethod(
            this,
            [this, token, owner] {
                Q_EMIT snapshotReceived(token, owner, snapshotWire(m_epoch, ++m_revision,
                                                                   m_value));
            },
            Qt::QueuedConnection);
    }

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

    // The test answers commits synchronously, mirroring the settings client
    // tests: the model sees exactly one deterministic reply per write.
    void replyToLastCommit(SettingsWireStatus status, const QVariant &value)
    {
        QVERIFY(!m_commits.isEmpty());
        const CommitRequest request = m_commits.takeLast();
        m_revision = request.baseRevision + 1;
        const quint64 after = status == SettingsWireStatus::Applied
            ? m_revision : request.baseRevision;
        m_commitStatus = status;
        m_value = value;
        Q_EMIT commitReceived(request.token, request.owner,
                              commitWire(request.baseRevision, after));
    }

    [[nodiscard]] int pendingCommitCount() const noexcept
    {
        return static_cast<int>(m_commits.size());
    }

    void announceOwner()
    {
        QMetaObject::invokeMethod(
            this, [this] { Q_EMIT ownerChanged(QStringLiteral(":1.7")); },
            Qt::QueuedConnection);
    }

    void setValue(const QVariant &value) { m_value = value; }
    void setCommitStatus(SettingsWireStatus status) { m_commitStatus = status; }

    [[nodiscard]] const QList<QVariantMap> &committedOperations() const noexcept
    {
        return m_committedOperations;
    }

    [[nodiscard]] static QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                                                  const QVariant &value)
    {
        return {{QLatin1StringView(WireContract::FieldStatus),
                 quint32(SettingsWireStatus::Applied)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), epoch},
                {QLatin1StringView(WireContract::FieldRevision), revision},
                {QLatin1StringView(WireContract::FieldValues),
                 QVariantMap{{QString::fromLatin1(kIdleKey), value}}},
                {QLatin1StringView(WireContract::FieldSourceLayers),
                 QVariantMap{{QString::fromLatin1(kIdleKey),
                              QStringLiteral("user-overrides")}}},
                {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

private:
    [[nodiscard]] QVariantMap commitWire(quint64 before, quint64 after) const
    {
        return {{QLatin1StringView(WireContract::FieldStatus), quint32(m_commitStatus)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), m_epoch},
                {QLatin1StringView(WireContract::FieldRevisionBefore), before},
                {QLatin1StringView(WireContract::FieldRevisionAfter), after},
                {QLatin1StringView(WireContract::FieldValues),
                 QVariantMap{{QString::fromLatin1(kIdleKey), m_value}}},
                {QLatin1StringView(WireContract::FieldSourceLayers),
                 QVariantMap{{QString::fromLatin1(kIdleKey),
                              QStringLiteral("user-overrides")}}},
                {QLatin1StringView(WireContract::FieldChangedKeys),
                 QStringList{QString::fromLatin1(kIdleKey)}},
                {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

    QString m_epoch = QStringLiteral("epoch-9");
    quint64 m_revision = 0;
    QVariant m_value = QVariant::fromValue<qint64>(20);
    SettingsWireStatus m_commitStatus = SettingsWireStatus::Applied;
    QList<QVariantMap> m_committedOperations;
    QList<CommitRequest> m_commits;
};

} // namespace

class IdleDisplaySettingsModelTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init();
    void initialTruthReflectsThePersistedSnapshot();
    void disablingWritesNeverAndReconcilesStatus();
    void enablingWithUnknownMinutesUsesTheDefault();
    void minutesAreBoundsCheckedBeforeWriting();
    void appliedCommitClearsBusyAndReportsStatus();
    void rejectedCommitReportsTheFailure();
    void busyModelRejectsFurtherWrites();
    void retryClearsErrorAndRefreshes();

private:
    std::unique_ptr<FakeSettingsTransport> m_transport;
    std::unique_ptr<SettingsClient> m_client;
    std::unique_ptr<Settings1IdlePreferences> m_preferences;
    std::unique_ptr<IdleDisplaySettingsModel> m_model;
};

void IdleDisplaySettingsModelTest::init() {
    m_client.reset();
    m_transport = std::make_unique<FakeSettingsTransport>();
    m_client = std::make_unique<SettingsClient>(
        *m_transport, Settings1IdlePreferences::scopedKey());
    m_preferences = std::make_unique<Settings1IdlePreferences>(*m_client);
    m_model = std::make_unique<IdleDisplaySettingsModel>(*m_preferences, *m_client);
    QString error;
    QVERIFY(m_client->start(&error));
}

void IdleDisplaySettingsModelTest::initialTruthReflectsThePersistedSnapshot() {
    m_transport->setValue(QVariant::fromValue<qint64>(20));
    m_transport->announceOwner();
    QTRY_COMPARE(m_model->minutes(), 20);
    QVERIFY(m_model->enabled());
    QVERIFY(m_model->statusText().contains(QStringLiteral("20")));
    QVERIFY(m_model->errorText().isEmpty());
}

void IdleDisplaySettingsModelTest::disablingWritesNeverAndReconcilesStatus() {
    m_transport->setValue(QVariant::fromValue<qint64>(20));
    m_transport->announceOwner();
    // Wait for confirmed truth, not the construction default, so the write
    // path sees a Ready client.
    QTRY_COMPARE(m_model->minutes(), 20);

    QVERIFY(m_model->setEnabled(false));
    QCOMPARE(m_transport->committedOperations().size(), 1);
    const QVariantMap operation = m_transport->committedOperations().constFirst();
    QCOMPARE(operation.value(QStringLiteral("key")).toString(),
             QStringLiteral("power.idleDisplayOffMinutes"));
    QCOMPARE(operation.value(QStringLiteral("value")).toLongLong(), -1);

    m_transport->replyToLastCommit(SettingsWireStatus::Applied,
                                   QVariant::fromValue<qint64>(-1));
    QTRY_VERIFY(!m_model->enabled());
    QVERIFY(!m_model->busy());
    QVERIFY(!m_model->statusText().contains(QStringLiteral("20")));
}

void IdleDisplaySettingsModelTest::enablingWithUnknownMinutesUsesTheDefault() {
    m_transport->setValue(QVariant::fromValue<qint64>(-1));
    m_transport->announceOwner();
    QTRY_VERIFY(!m_model->enabled());

    QVERIFY(m_model->setEnabled(true));
    m_transport->replyToLastCommit(SettingsWireStatus::Applied,
                                   QVariant::fromValue<qint64>(
                                       IdleDisplayPreferences::defaultTimeoutMinutes()));
    QCOMPARE(m_transport->committedOperations().constFirst()
                 .value(QStringLiteral("value"))
                 .toLongLong(),
             IdleDisplayPreferences::defaultTimeoutMinutes());
}

void IdleDisplaySettingsModelTest::minutesAreBoundsCheckedBeforeWriting() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(!m_model->setMinutes(0));
    QVERIFY(!m_model->setMinutes(241));
    QVERIFY(!m_model->errorText().isEmpty());
    QCOMPARE(m_transport->committedOperations().size(), 0);
    QVERIFY(m_model->setMinutes(240));
    QCOMPARE(m_transport->committedOperations().size(), 1);
    m_transport->replyToLastCommit(SettingsWireStatus::Applied,
                                   QVariant::fromValue<qint64>(240));
    QTRY_VERIFY(!m_model->busy());
}

void IdleDisplaySettingsModelTest::appliedCommitClearsBusyAndReportsStatus() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->setMinutes(15));
    QVERIFY(m_model->busy());
    m_transport->replyToLastCommit(SettingsWireStatus::Applied,
                                   QVariant::fromValue<qint64>(15));
    QVERIFY(!m_model->busy());
    QVERIFY2(m_model->errorText().isEmpty(), qPrintable(m_model->errorText()));
}

void IdleDisplaySettingsModelTest::rejectedCommitReportsTheFailure() {
    m_transport->setCommitStatus(SettingsWireStatus::PersistenceFailed);
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->setMinutes(15));
    m_transport->replyToLastCommit(SettingsWireStatus::PersistenceFailed,
                                   QVariant::fromValue<qint64>(20));
    QVERIFY(!m_model->busy());
    QVERIFY(!m_model->errorText().isEmpty());
}

void IdleDisplaySettingsModelTest::busyModelRejectsFurtherWrites() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->setMinutes(15));
    QVERIFY(m_model->busy());
    QVERIFY(!m_model->setMinutes(30));
    QCOMPARE(m_transport->committedOperations().size(), 1);
    QCOMPARE(m_transport->pendingCommitCount(), 1);
}

void IdleDisplaySettingsModelTest::retryClearsErrorAndRefreshes() {
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QVERIFY(m_model->retry());
    QVERIFY(m_model->errorText().isEmpty());
}

QTEST_MAIN(IdleDisplaySettingsModelTest)
#include "tst_idle_display_settings.moc"
