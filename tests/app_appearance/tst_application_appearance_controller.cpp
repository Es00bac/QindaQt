// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QStyleHints>
#include <QtTest>
#include <qindaqt/app_appearance/application_appearance_controller.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

using namespace QindaQt::AppAppearance;
using namespace QindaQt::Services::SettingsClient;
namespace WC = QindaQt::Services::SettingsProtocol;

class FakeTransport final : public SettingsTransport {
  Q_OBJECT
public:
  bool start(QString *) override { return true; }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &) override {
    lastToken = token;
    lastOwner = owner;
  }
  void commit(quint64, const QString &, const QString &, quint64,
              const QVariantList &) override {}
  void requestActivation() override {}
  void announce(const QString &owner) { emit ownerChanged(owner); }
  void invalidate(quint64 revision) {
    emit settingsChanged(lastOwner, QStringLiteral("epoch"), revision,
                         {QStringLiteral("appearance.theme"),
                          QStringLiteral("appearance.colorScheme")});
  }
  void reply(QString theme, QString scheme, quint64 revision = 1) {
    const QVariantMap values{
        {QStringLiteral("appearance.theme"), theme},
        {QStringLiteral("appearance.colorScheme"), scheme}};
    const QVariantMap wire{
        {QLatin1StringView(WC::WireContract::FieldStatus),
         quint32(WC::SettingsWireStatus::Applied)},
        {QLatin1StringView(WC::WireContract::FieldWireSchemaVersion),
         WC::WireContract::WireSchemaVersion},
        {QLatin1StringView(WC::WireContract::FieldSettingsSchemaVersion),
         quint32(2)},
        {QLatin1StringView(WC::WireContract::FieldEpoch),
         QStringLiteral("epoch")},
        {QLatin1StringView(WC::WireContract::FieldRevision), revision},
        {QLatin1StringView(WC::WireContract::FieldValues), values},
        {QLatin1StringView(WC::WireContract::FieldSourceLayers),
         QVariantMap{{QStringLiteral("appearance.theme"),
                      QStringLiteral("user-overrides")},
                     {QStringLiteral("appearance.colorScheme"),
                      QStringLiteral("user-overrides")}}},
        {QLatin1StringView(WC::WireContract::FieldMessage), QString{}}};
    emit snapshotReceived(lastToken, lastOwner, wire);
  }
  quint64 lastToken = 0;
  QString lastOwner;
};

class ApplicationAppearanceControllerTest final : public QObject {
  Q_OBJECT
private slots:
  void snapshotUpdatesInvalidRetainsAndSystemRefreshes();
  void explicitOverrideIgnoresSettings();
};

static QStringList themeDirectories() {
  return {QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes")};
}

void ApplicationAppearanceControllerTest::
    snapshotUpdatesInvalidRetainsAndSystemRefreshes() {
  FakeTransport transport;
  SettingsClient client(transport, {QStringLiteral("appearance.theme"),
                                    QStringLiteral("appearance.colorScheme")});
  QStyleHints *hints = QGuiApplication::styleHints();
  QVERIFY(hints != nullptr);
  ApplicationAppearanceController controller(client, themeDirectories(),
                                             QStringLiteral("qinda-dark"));
  QSignalSpy changes(&controller,
                     &ApplicationAppearanceController::appearanceChanged);
  QVERIFY(client.start());
  transport.announce(QStringLiteral(":1.20"));
  QTRY_VERIFY(transport.lastToken != 0);
  transport.reply(QStringLiteral("qinda-dark"), QStringLiteral("system"));
  QTRY_COMPARE(client.state(), ClientState::Ready);
  QVERIFY(client.snapshot().has_value());
  Q_EMIT hints->colorSchemeChanged(Qt::ColorScheme::Light);
  QCOMPARE(controller.themeId(), QStringLiteral("qinda-light"));
  QCOMPARE(changes.count(), 1);
  const quint64 baselineToken = transport.lastToken;
  transport.invalidate(2);
  QTRY_VERIFY(transport.lastToken != baselineToken);
  transport.reply(QStringLiteral("qinda-dark"), QStringLiteral("system"), 2);
  QCOMPARE(changes.count(), 1);
  Q_EMIT hints->colorSchemeChanged(Qt::ColorScheme::Dark);
  QTRY_COMPARE(controller.themeId(), QStringLiteral("qinda-dark"));
  const quint64 duplicateToken = transport.lastToken;
  transport.invalidate(3);
  QTRY_VERIFY(transport.lastToken != duplicateToken);
  transport.reply(QStringLiteral("missing"), QStringLiteral("dark"), 3);
  QCOMPARE(controller.themeId(), QStringLiteral("qinda-dark"));
  const quint64 missingToken = transport.lastToken;
  transport.invalidate(4);
  QTRY_VERIFY(transport.lastToken != missingToken);
  transport.reply(QStringLiteral("qinda-light"), QStringLiteral("sepia"), 4);
  QCOMPARE(controller.themeId(), QStringLiteral("qinda-dark"));
  QCOMPARE(changes.count(), 2);
}

void ApplicationAppearanceControllerTest::explicitOverrideIgnoresSettings() {
  FakeTransport transport;
  SettingsClient client(transport, {QStringLiteral("appearance.theme"),
                                    QStringLiteral("appearance.colorScheme")});
  ApplicationAppearanceController controller(client, themeDirectories(),
                                             QStringLiteral("qinda-dark"),
                                             QStringLiteral("qinda-dusk"));
  QSignalSpy changes(&controller,
                     &ApplicationAppearanceController::appearanceChanged);
  QVERIFY(client.start());
  transport.announce(QStringLiteral(":1.21"));
  QTRY_VERIFY(transport.lastToken != 0);
  transport.reply(QStringLiteral("qinda-light"), QStringLiteral("light"));
  QTRY_COMPARE(client.state(), ClientState::Ready);
  QCOMPARE(controller.themeId(), QStringLiteral("qinda-dusk"));
  QCOMPARE(changes.count(), 0);
}

QTEST_MAIN(ApplicationAppearanceControllerTest)
#include "tst_application_appearance_controller.moc"
