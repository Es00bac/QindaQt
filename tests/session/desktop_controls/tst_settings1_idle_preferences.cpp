// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/session/desktop_controls/settings1_idle_preferences.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include <QtTest>

#include <memory>

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

    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &) override
    {
        ++m_commits;
    }
    void requestActivation() override {}

    [[nodiscard]] int commits() const noexcept { return m_commits; }

    void announceOwner()
    {
        QMetaObject::invokeMethod(
            this, [this] { Q_EMIT ownerChanged(QStringLiteral(":1.5")); },
            Qt::QueuedConnection);
    }

    void setValue(const QVariant &value) { m_value = value; }

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
    QString m_epoch = QStringLiteral("epoch-1");
    quint64 m_revision = 0;
    int m_commits = 0;
    QVariant m_value = QVariant::fromValue<qint64>(10);
};

} // namespace

class Settings1IdlePreferencesTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void noOwnerYetKeepsDocumentedDefault();
    void persistedValueMapsToBoundedPreferences();
    void persistedNeverValueDisablesPolicy();
    void valueChangeIsSignalled();
    void sameValueChangeStaysQuiet();
    void refreshNeverWrites();

private:
    void rebuildClient();

    std::unique_ptr<FakeSettingsTransport> m_transport;
    std::unique_ptr<SettingsClient> m_client;
};

void Settings1IdlePreferencesTest::rebuildClient() {
    // AGENT-GUARD: destroy the client before the transport it borrows;
    // reversing this order stops a client against freed transport memory.
    m_client.reset();
    m_transport = std::make_unique<FakeSettingsTransport>();
    m_client = std::make_unique<SettingsClient>(*m_transport,
                                                Settings1IdlePreferences::scopedKey());
}

void Settings1IdlePreferencesTest::noOwnerYetKeepsDocumentedDefault() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));

    Settings1IdlePreferences preferences(*m_client);
    // Construction default: the documented ten minutes, enabled.
    QCOMPARE(preferences.currentPreferences(), IdleDisplayPreferences(true, 10));
    QCOMPARE(Settings1IdlePreferences::scopedKey(),
             QStringList{QStringLiteral("power.idleDisplayOffMinutes")});
}

void Settings1IdlePreferencesTest::persistedValueMapsToBoundedPreferences() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(QVariant::fromValue<qint64>(25));

    Settings1IdlePreferences preferences(*m_client);
    m_transport->announceOwner();
    QTRY_COMPARE(preferences.currentPreferences(), IdleDisplayPreferences(true, 25));
}

void Settings1IdlePreferencesTest::persistedNeverValueDisablesPolicy() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(QVariant::fromValue<qint64>(-1));

    Settings1IdlePreferences preferences(*m_client);
    m_transport->announceOwner();
    QTRY_COMPARE(preferences.currentPreferences(), IdleDisplayPreferences(false, 0));
}

void Settings1IdlePreferencesTest::valueChangeIsSignalled() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(QVariant::fromValue<qint64>(25));

    Settings1IdlePreferences preferences(*m_client);
    QSignalSpy spy(&preferences, &IdlePreferencesProvider::preferencesChanged);
    m_transport->announceOwner();
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<IdleDisplayPreferences>(),
             IdleDisplayPreferences(true, 25));
}

void Settings1IdlePreferencesTest::sameValueChangeStaysQuiet() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(QVariant::fromValue<qint64>(10));

    Settings1IdlePreferences preferences(*m_client);
    m_transport->announceOwner();
    QTRY_VERIFY(m_client->snapshot().has_value());
    // The initial default equals the persisted value: no preferencesChanged.
    QTRY_COMPARE(preferences.currentPreferences(), IdleDisplayPreferences(true, 10));

    QSignalSpy spy(&preferences, &IdlePreferencesProvider::preferencesChanged);
    m_client->refresh();
    QTRY_VERIFY(m_client->snapshot().has_value());
    QCOMPARE(preferences.currentPreferences(), IdleDisplayPreferences(true, 10));
    QCOMPARE(spy.count(), 0);
}

void Settings1IdlePreferencesTest::refreshNeverWrites() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));

    Settings1IdlePreferences preferences(*m_client);
    preferences.refresh();
    // The provider is read-only: refresh only re-reads, it never commits.
    QCOMPARE(m_transport->commits(), 0);
}

QTEST_MAIN(Settings1IdlePreferencesTest)
#include "tst_settings1_idle_preferences.moc"
