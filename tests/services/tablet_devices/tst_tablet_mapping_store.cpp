// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/tablet_devices/tablet_mapping_store.h>

#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::TabletDevices;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

constexpr auto kKey = "input.tabletMappings";

class FakeSettingsTransport final : public SettingsTransport {
    Q_OBJECT

public:
    bool start(QString *error) override {
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }
    void stop() override {}

    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &keys) override {
        m_requestedKeys = keys;
        QMetaObject::invokeMethod(
            this,
            [this, token, owner] {
                Q_EMIT snapshotReceived(
                    token, owner, snapshotWire(m_epoch, ++m_revision, m_value));
            },
            Qt::QueuedConnection);
    }

    void commit(quint64, const QString &, const QString &, quint64,
                const QVariantList &writes) override {
        ++m_commits;
        m_lastCommit = writes;
    }
    void requestActivation() override {}

    [[nodiscard]] int commits() const noexcept { return m_commits; }
    [[nodiscard]] QVariantList lastCommit() const { return m_lastCommit; }
    [[nodiscard]] QStringList requestedKeys() const { return m_requestedKeys; }

    void setValue(const QVariant &value) { m_value = value; }

    void announceOwner() {
        QMetaObject::invokeMethod(
            this, [this] { Q_EMIT ownerChanged(QStringLiteral(":1.5")); },
            Qt::QueuedConnection);
    }

    [[nodiscard]] static QVariantMap snapshotWire(const QString &epoch,
                                                  quint64 revision,
                                                  const QVariant &value) {
        return {{QLatin1StringView(WireContract::FieldStatus),
                 quint32(SettingsWireStatus::Applied)},
                {QLatin1StringView(WireContract::FieldWireSchemaVersion),
                 WireContract::WireSchemaVersion},
                {QLatin1StringView(WireContract::FieldSettingsSchemaVersion),
                 quint32(2)},
                {QLatin1StringView(WireContract::FieldEpoch), epoch},
                {QLatin1StringView(WireContract::FieldRevision), revision},
                {QLatin1StringView(WireContract::FieldValues),
                 QVariantMap{{QString::fromLatin1(kKey), value}}},
                {QLatin1StringView(WireContract::FieldSourceLayers),
                 QVariantMap{{QString::fromLatin1(kKey),
                              QStringLiteral("user-overrides")}}},
                {QLatin1StringView(WireContract::FieldMessage), QString{}}};
    }

private:
    QString m_epoch = QStringLiteral("epoch-1");
    quint64 m_revision = 0;
    int m_commits = 0;
    QVariantList m_lastCommit;
    QStringList m_requestedKeys;
    QVariant m_value = QVariant(QVariantMap{});
};

QVariantMap oneRecord(const QString &output, bool userChosen) {
    return QVariantMap{
        {QStringLiteral("1386:934:Wacom One Pen Display 13"),
         QVariantMap{{QStringLiteral("choice"), QStringLiteral("output")},
                     {QStringLiteral("outputName"), output},
                     {QStringLiteral("userChosen"), userChosen},
                     {QStringLiteral("announced"), true}}}};
}

} // namespace

class Settings1TabletMappingsTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void theClientIsScopedToExactlyOneKey();
    void noOwnerYetLeavesTheStoreUnloaded();
    void aPersistedLedgerLoadsAndPublishes();
    void savingCommitsTheWholeDocument();
    void recordChoiceTouchesOneDeviceAndNeverClearsAUserChoice();

private:
    void rebuildClient();

    std::unique_ptr<FakeSettingsTransport> m_transport;
    std::unique_ptr<SettingsClient> m_client;
};

void Settings1TabletMappingsTest::rebuildClient() {
    // AGENT-GUARD: destroy the client before the transport it borrows;
    // reversing this order stops a client against freed transport memory.
    m_client.reset();
    m_transport = std::make_unique<FakeSettingsTransport>();
    m_client = std::make_unique<SettingsClient>(
        *m_transport, Settings1TabletMappings::scopedKey());
}

void Settings1TabletMappingsTest::theClientIsScopedToExactlyOneKey() {
    // AGENT-GUARD: Settings1 rejects a whole snapshot on one unknown key
    // (ADR-0126). A widened scope would put an unrelated feature's key at
    // risk of this one's schema.
    QCOMPARE(Settings1TabletMappings::scopedKey(),
             QStringList{QStringLiteral("input.tabletMappings")});
}

void Settings1TabletMappingsTest::noOwnerYetLeavesTheStoreUnloaded() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    Settings1TabletMappings store(*m_client);
    // Unloaded is the honest state: the policy must not act on a ledger it
    // has not read, or it could override a choice the user already made.
    QVERIFY(!store.isLoaded());
    QVERIFY(store.ledger().isEmpty());
}

void Settings1TabletMappingsTest::aPersistedLedgerLoadsAndPublishes() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->setValue(
        QVariant(oneRecord(QStringLiteral("HDMI-A-1"), true)));

    Settings1TabletMappings store(*m_client);
    QSignalSpy changed(&store, &Settings1TabletMappings::ledgerChanged);
    m_transport->announceOwner();
    QTRY_VERIFY(store.isLoaded());
    QTRY_COMPARE(changed.size(), 1);
    const TabletMappingRecord record =
        store.ledger().record(QStringLiteral("1386:934:Wacom One Pen Display 13"));
    QCOMPARE(record.choice, TabletMapChoice::NamedOutput);
    QCOMPARE(record.outputName, QStringLiteral("HDMI-A-1"));
    QVERIFY(record.userChosen);
}

void Settings1TabletMappingsTest::savingCommitsTheWholeDocument() {
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    Settings1TabletMappings store(*m_client);
    m_transport->announceOwner();
    QTRY_VERIFY(store.isLoaded());

    TabletMappingLedger ledger;
    TabletMappingRecord record;
    record.choice = TabletMapChoice::NamedOutput;
    record.outputName = QStringLiteral("DP-2");
    record.announced = true;
    ledger.setRecord(QStringLiteral("1386:934:Wacom One Pen Display 13"), record);
    QVERIFY(store.save(ledger));

    // The local copy moves immediately so a policy pass reading back mid
    // write sees what it just decided.
    QCOMPARE(store.ledger().record(QStringLiteral("1386:934:Wacom One Pen Display 13")).outputName,
             QStringLiteral("DP-2"));
    QTRY_COMPARE(m_transport->commits(), 1);
}

void Settings1TabletMappingsTest::recordChoiceTouchesOneDeviceAndNeverClearsAUserChoice() {
    // Both the session policy and the Settings route record through this one
    // method, so a choice made in one place cannot drop the other's record.
    rebuildClient();
    QString error;
    QVERIFY(m_client->start(&error));
    Settings1TabletMappings store(*m_client);
    m_transport->announceOwner();
    QTRY_VERIFY(store.isLoaded());

    QVERIFY(store.recordChoice(QStringLiteral("1386:934:Wacom One Pen Display 13"),
                               TabletMapChoice::NamedOutput,
                               QStringLiteral("HDMI-A-1"), true,
                               QStringLiteral("Wacom One Pen Display 13")));
    QVERIFY(store.recordChoice(QStringLiteral("1386:999:Other Tablet"),
                               TabletMapChoice::FollowActiveScreen, {}, false,
                               QStringLiteral("Other Tablet")));
    QCOMPARE(store.ledger().size(), 2);
    QVERIFY(store.ledger()
                .record(QStringLiteral("1386:934:Wacom One Pen Display 13"))
                .userChosen);

    // AGENT-GUARD: an automatic pass must never clear a decision the user
    // made, so userChosen only ever goes false -> true.
    QVERIFY(store.recordChoice(QStringLiteral("1386:934:Wacom One Pen Display 13"),
                               TabletMapChoice::NamedOutput,
                               QStringLiteral("DP-2"), false,
                               QStringLiteral("Wacom One Pen Display 13")));
    const auto record =
        store.ledger().record(QStringLiteral("1386:934:Wacom One Pen Display 13"));
    QVERIFY(record.userChosen);
    QCOMPARE(record.outputName, QStringLiteral("DP-2"));

    // A device with nothing stable to key on is refused rather than stored
    // under a key that could never be found again.
    QVERIFY(!store.recordChoice(QString(), TabletMapChoice::NamedOutput,
                                QStringLiteral("HDMI-A-1"), true, QString()));
}

QTEST_MAIN(Settings1TabletMappingsTest)
#include "tst_tablet_mapping_store.moc"
