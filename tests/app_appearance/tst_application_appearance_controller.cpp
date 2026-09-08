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
    QVariantMap values{
        {QStringLiteral("appearance.theme"), theme},
        {QStringLiteral("appearance.colorScheme"), scheme}};
    for (auto it = extra.cbegin(); it != extra.cend(); ++it) values.insert(it.key(), it.value());
    QVariantMap sources;
    for (auto it = values.cbegin(); it != values.cend(); ++it) sources.insert(it.key(), QStringLiteral("user-overrides"));
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
         sources},
        {QLatin1StringView(WC::WireContract::FieldMessage), QString{}}};
    emit snapshotReceived(lastToken, lastOwner, wire);
  }
  QVariantMap extra;
  quint64 lastToken = 0;
  QString lastOwner;
};

class ApplicationAppearanceControllerTest final : public QObject {
  Q_OBJECT
private slots:
  void snapshotUpdatesInvalidRetainsAndSystemRefreshes();
  void explicitOverrideIgnoresSettings();
  void explicitPaletteStillHonorsReadability();
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
  // System preserves the selected installed theme (ADR-0080); ambient host
  // palette changes must not replace the user's QindaQt theme.
  QCOMPARE(controller.themeId(), QStringLiteral("qinda-dark"));
  QCOMPARE(changes.count(), 0);
  const quint64 baselineToken = transport.lastToken;
  transport.invalidate(2);
  QTRY_VERIFY(transport.lastToken != baselineToken);
  transport.reply(QStringLiteral("qinda-dark"), QStringLiteral("light"), 2);
  QCOMPARE(controller.themeId(), QStringLiteral("qinda-light"));
  QCOMPARE(changes.count(), 1);
  Q_EMIT hints->colorSchemeChanged(Qt::ColorScheme::Dark);
  QTRY_COMPARE(controller.themeId(), QStringLiteral("qinda-light"));
  const quint64 duplicateToken = transport.lastToken;
  transport.invalidate(3);
  QTRY_VERIFY(transport.lastToken != duplicateToken);
  // An unavailable requested ID with an explicit scheme resolves to the
  // installed compatible variant; malformed scheme tokens retain that value.
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

void ApplicationAppearanceControllerTest::explicitPaletteStillHonorsReadability() {
  FakeTransport transport;
  const QStringList keys{QStringLiteral("appearance.theme"), QStringLiteral("appearance.colorScheme"),
    QStringLiteral("fonts.family"), QStringLiteral("fonts.monospaceFamily"),
    QStringLiteral("fonts.pointSize"), QStringLiteral("accessibility.textScale"),
    QStringLiteral("accessibility.reducedTransparency"), QStringLiteral("accessibility.highContrast")};
  SettingsClient client(transport, keys);
  ApplicationAppearanceController controller(client, themeDirectories(),
    QStringLiteral("qinda-dark"), QStringLiteral("qinda-dusk"));
  QVERIFY(client.start());
  transport.announce(QStringLiteral(":1.25"));
  QTRY_VERIFY(transport.lastToken != 0);
  transport.extra = {{QStringLiteral("fonts.family"), QStringLiteral("Noto Sans")},
    {QStringLiteral("fonts.monospaceFamily"), QStringLiteral("Noto Sans Mono")},
    {QStringLiteral("fonts.pointSize"), 14.0}, {QStringLiteral("accessibility.textScale"), 1.5},
    {QStringLiteral("accessibility.reducedTransparency"), true},
    {QStringLiteral("accessibility.highContrast"), true}};
  transport.reply(QStringLiteral("qinda-light"), QStringLiteral("light"));
  QTRY_COMPARE(client.state(), ClientState::Ready);
  QCOMPARE(controller.themeId(), QStringLiteral("qinda-dusk"));
  QCOMPARE(controller.theme().fontFamily, QStringLiteral("Noto Sans"));
  QCOMPARE(controller.theme().monoFontFamily, QStringLiteral("Noto Sans Mono"));
  QCOMPARE(controller.accessibilityInputs().basePointSize, 14.0);
  QCOMPARE(controller.accessibilityInputs().textScale, 1.5);
  QVERIFY(controller.accessibilityInputs().reducedTransparency);
  QVERIFY(controller.accessibilityInputs().highContrast);
  transport.announce(QString());
  QCOMPARE(controller.accessibilityInputs().basePointSize, 14.0);
}

QTEST_MAIN(ApplicationAppearanceControllerTest)
#include "tst_application_appearance_controller.moc"
