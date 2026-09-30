#include <qindaqt/apps/settings_screen_lock/settings1_screen_lock_settings_model.h>
#include <qindaqt/services/lock_preferences/lock_preferences.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <QtTest>
#include <memory>
#include "../screensaver/screensaver_model_test_support.h"

using namespace QindaQt::Apps::SettingsScreenLock;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using ScreensaverTestSupport::FakeSettingsTransport;

class Settings1ScreenLockSettingsModelTest final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void init();
    void confirmedPreferencesAreDisplayedAndEditedThroughSettings1();
    void unavailableOrInvalidEditsDoNotWrite();
private:
    std::unique_ptr<FakeSettingsTransport> m_transport;
    std::unique_ptr<SettingsClient> m_client;
    std::unique_ptr<QindaQt::Services::LockPreferences::PreferencesProvider> m_provider;
    std::unique_ptr<Settings1ScreenLockSettingsModel> m_model;
};

void Settings1ScreenLockSettingsModelTest::init() {
    m_model.reset();
    m_provider.reset();
    m_client.reset();
    m_transport = std::make_unique<FakeSettingsTransport>();
    m_client = std::make_unique<SettingsClient>(
        *m_transport, QindaQt::Services::LockPreferences::scopedKeys());
    m_provider = std::make_unique<QindaQt::Services::LockPreferences::PreferencesProvider>(
        *m_client);
    m_model = std::make_unique<Settings1ScreenLockSettingsModel>(
        *m_client, *m_provider);
}

void Settings1ScreenLockSettingsModelTest::
confirmedPreferencesAreDisplayedAndEditedThroughSettings1() {
    m_transport->setValue("lock.automaticEnabled", false);
    m_transport->setValue("lock.idleTimeoutSeconds", QVariant::fromValue<qint64>(900));
    m_transport->setValue("lock.onResume", false);
    m_transport->setValue("lock.graceSeconds", QVariant::fromValue<qint64>(30));
    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->announceOwner();
    QTRY_VERIFY_WITH_TIMEOUT(m_client->snapshot().has_value(), 4000);
    QTRY_VERIFY_WITH_TIMEOUT(!m_model->busy(), 4000);
    QVERIFY(!m_model->automaticLock());
    QCOMPARE(m_model->timeoutMinutes(), 15);
    QVERIFY(!m_model->lockOnResume());
    QCOMPARE(m_model->lockGraceSeconds(), 30);

    QVERIFY(m_model->setTimeoutMinutes(20));
    QVERIFY(m_model->busy());
    QCOMPARE(m_transport->committedOperations().size(), 1);
    const auto operation = m_transport->committedOperations().constFirst();
    QCOMPARE(operation.value(QStringLiteral("key")).toString(),
             QStringLiteral("lock.idleTimeoutSeconds"));
    QCOMPARE(operation.value(QStringLiteral("value")).toLongLong(), 1200);
    m_transport->replyToLastCommit(SettingsWireStatus::Applied);
    QTRY_VERIFY(!m_model->busy());
    QCOMPARE(m_model->timeoutMinutes(), 20);
    QVERIFY(m_model->statusText().contains(QStringLiteral("saved"),
                                          Qt::CaseInsensitive));
    m_client->stop();
}

void Settings1ScreenLockSettingsModelTest::unavailableOrInvalidEditsDoNotWrite() {
    m_transport->setValue("lock.automaticEnabled", true);
    m_transport->setValue("lock.idleTimeoutSeconds", QVariant::fromValue<qint64>(300));
    m_transport->setValue("lock.onResume", true);
    m_transport->setValue("lock.graceSeconds", QVariant::fromValue<qint64>(5));
    QVERIFY(!m_model->setAutomaticLock(false));
    QCOMPARE(m_transport->committedOperations().size(), 0);

    QString error;
    QVERIFY(m_client->start(&error));
    m_transport->announceOwner();
    QTRY_VERIFY(!m_model->busy());
    QVERIFY(!m_model->setTimeoutMinutes(241));
    QVERIFY(!m_model->setLockGraceSeconds(1));
    QCOMPARE(m_transport->committedOperations().size(), 0);
    QVERIFY(!m_model->errorText().isEmpty());
    m_client->stop();
}

QTEST_GUILESS_MAIN(Settings1ScreenLockSettingsModelTest)
#include "tst_settings1_screen_lock_settings_model.moc"
