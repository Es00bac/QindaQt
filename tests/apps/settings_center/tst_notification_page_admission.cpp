// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/apps/settings/notifications/notification_schedule_model.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/apps/settings_notifications/notification_application_settings_model.h"
#include "qindaqt/services/settings_client/do_not_disturb_controller.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/themes/theme_loader.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>
#include <QUrl>

using QindaQt::Apps::SettingsNotifications::NotificationScheduleModel;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {
const QString Dnd = QStringLiteral("services.doNotDisturb");
const QString Schedule = QStringLiteral("services.doNotDisturbSchedule");
const QString Start = QStringLiteral("services.doNotDisturbStartMinutes");
const QString End = QStringLiteral("services.doNotDisturbEndMinutes");
const QString ApplicationPolicies = QStringLiteral("services.notificationPolicies");
const QString ApplicationId = QStringLiteral("org.example.PolicyTest");
const QString Owner = QStringLiteral(":1.71");
const QString Epoch = QStringLiteral("notification-epoch-a");

class FakeTransport final : public SettingsTransport {
public:
  struct Request {
    quint64 token = 0;
    QString owner;
    QString key;
  };
  bool start(QString *) override { return true; }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &) override {
    snapshots.append({token, owner, {}});
  }
  void commit(quint64 token, const QString &owner, const QString &, quint64,
              const QVariantList &operations) override {
    const QString key = operations.isEmpty()
        ? QString{} : operations.first().toMap().value(QStringLiteral("key")).toString();
    commits.append({token, owner, key});
  }
  void requestActivation() override {}
  QList<Request> snapshots;
  QList<Request> commits;
};

QVariantMap values() {
  return {{Dnd, false}, {Schedule, false}, {Start, 1320}, {End, 420},
          {ApplicationPolicies,
           QVariantMap{{ApplicationId,
                        QVariantMap{{QStringLiteral("muted"), true},
                                    {QStringLiteral("soundEnabled"), true}}}}}};
}

QVariantMap sources(const QVariantMap &values) {
  QVariantMap result;
  for (auto it = values.cbegin(); it != values.cend(); ++it)
    result.insert(it.key(), QStringLiteral("system-defaults"));
  return result;
}

QVariantMap snapshotWire() {
  const QVariantMap confirmed = values();
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), Epoch},
          {QLatin1StringView(WireContract::FieldRevision), quint64(1)},
          {QLatin1StringView(WireContract::FieldValues), confirmed},
          {QLatin1StringView(WireContract::FieldSourceLayers), sources(confirmed)},
          {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

QVariantMap refusedCommitWire(const QString &key) {
  const QVariantMap allValues = values();
  const QVariantMap confirmed{{key, allValues.value(key)}};
  const QVariantMap confirmedSources{{key, QStringLiteral("system-defaults")}};
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::ReadOnlyLayer)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), Epoch},
          {QLatin1StringView(WireContract::FieldRevisionBefore), quint64(1)},
          {QLatin1StringView(WireContract::FieldRevisionAfter), quint64(1)},
          {QLatin1StringView(WireContract::FieldValues), confirmed},
          {QLatin1StringView(WireContract::FieldSourceLayers), confirmedSources},
          {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{}},
          {QLatin1StringView(WireContract::FieldMessage),
           QStringLiteral("DND policy blocked this change")}};
}

QQuickItem *findQuickItem(QQuickItem *root, const QString &objectName) {
  if (!root)
    return nullptr;
  if (root->objectName() == objectName)
    return root;
  for (auto *child : root->childItems()) {
    if (auto *match = findQuickItem(child, objectName))
      return match;
  }
  return nullptr;
}
} // namespace

class NotificationPageAdmissionTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void refreshAndRefusalKeepBothSwitchesAuthoritative();
};

void NotificationPageAdmissionTest::refreshAndRefusalKeepBothSwitchesAuthoritative() {
  FakeTransport transport;
  SettingsClient client(transport, {Dnd, Schedule, Start, End, ApplicationPolicies},
                        {.requestTimeoutMilliseconds = 500,
                         .debounceMilliseconds = 0,
                         .retryMilliseconds = {10}});
  NotificationScheduleModel schedule(client);
  DoNotDisturbController dnd(client);
  QindaQt::Apps::SettingsNotifications::NotificationApplicationSettingsModel
      applicationPolicies(client, {{ApplicationId, QStringLiteral("Policy Test App"),
                                   QStringLiteral("application-x-executable")}});
  QVERIFY(client.start());
  Q_EMIT transport.ownerChanged(Owner);
  QTRY_COMPARE(transport.snapshots.size(), 1);
  const auto first = transport.snapshots.takeFirst();
  Q_EMIT transport.snapshotReceived(first.token, first.owner, snapshotWire());
  QVERIFY(dnd.ready());
  QVERIFY(schedule.canEdit());
  QVERIFY(applicationPolicies.available());
  QCOMPARE(applicationPolicies.rowCount(), 1);

  QQuickView view;
  view.engine()->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
  QString facadeError;
  auto *facade =
      QindaQt::Apps::SettingsAppearance::ensureTokenFacade(*view.engine(), &facadeError);
  QVERIFY2(facade != nullptr, qPrintable(facadeError));
  const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
      QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
  QVERIFY2(theme.ok, qPrintable(theme.error));
  QString publishError;
  QVERIFY2(facade->publish(theme.theme, {}, &publishError), qPrintable(publishError));
  view.setInitialProperties({
      {QStringLiteral("quietingSettings"), QVariant::fromValue(static_cast<QObject *>(&dnd))},
      {QStringLiteral("quietingSchedule"), QVariant::fromValue(static_cast<QObject *>(&schedule))},
      {QStringLiteral("applicationPolicies"), QVariant::fromValue(static_cast<QObject *>(&applicationPolicies))}});
  view.setSource(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR "/NotificationsPage.qml")));
  QVERIFY2(view.status() == QQuickView::Ready, qPrintable(view.errors().isEmpty()
      ? QString{} : view.errors().constFirst().toString()));
  view.resize(900, 900);
  view.show();
  QVERIFY(QTest::qWaitForWindowExposed(&view));
  auto *dndSwitch = view.rootObject()->findChild<QQuickItem *>(
      QStringLiteral("settingsDoNotDisturbSwitch"));
  auto *scheduleSwitch = view.rootObject()->findChild<QQuickItem *>(
      QStringLiteral("settingsQuietHoursSwitch"));
  QTRY_VERIFY(findQuickItem(view.rootObject(),
                            QStringLiteral("notificationMute-") + ApplicationId) != nullptr);
  auto *muteSwitch = findQuickItem(view.rootObject(),
                                   QStringLiteral("notificationMute-") + ApplicationId);
  auto *soundSwitch = findQuickItem(view.rootObject(),
                                    QStringLiteral("notificationSound-") + ApplicationId);
  QVERIFY(dndSwitch != nullptr);
  QVERIFY(scheduleSwitch != nullptr);
  QVERIFY(muteSwitch != nullptr);
  QVERIFY(soundSwitch != nullptr);
  QVERIFY(dndSwitch->isEnabled());
  QVERIFY(scheduleSwitch->isEnabled());
  QVERIFY(!dndSwitch->property("checked").toBool());
  QVERIFY(!scheduleSwitch->property("checked").toBool());
  QVERIFY(muteSwitch->property("checked").toBool());
  QVERIFY(soundSwitch->property("checked").toBool());

  client.refresh();
  QTRY_COMPARE(transport.snapshots.size(), 1);
  // The client's public state remains Ready while its serial request lane is
  // occupied. The actual route must disable both controls immediately.
  QVERIFY(!dndSwitch->isEnabled());
  QVERIFY(!scheduleSwitch->isEnabled());
  QVERIFY(!muteSwitch->isEnabled());
  QVERIFY(!soundSwitch->isEnabled());
  muteSwitch->forceActiveFocus();
  QTest::keyClick(&view, Qt::Key_Space);
  QCoreApplication::processEvents();
  QVERIFY(transport.commits.isEmpty());
  QVERIFY(muteSwitch->property("checked").toBool());

  const auto refresh = transport.snapshots.takeFirst();
  Q_EMIT transport.snapshotReceived(refresh.token, refresh.owner, snapshotWire());
  QTRY_VERIFY(dndSwitch->isEnabled());
  QTRY_VERIFY(scheduleSwitch->isEnabled());
  QTRY_VERIFY(muteSwitch->isEnabled());
  QTRY_VERIFY(soundSwitch->isEnabled());
  QVERIFY(!dndSwitch->property("checked").toBool());
  QVERIFY(!scheduleSwitch->property("checked").toBool());
  QVERIFY(muteSwitch->property("checked").toBool());
  QVERIFY(soundSwitch->property("checked").toBool());

  // A real pointer click requests a save, but the displayed value remains the
  // last exact Settings1 value while the request is pending and after refusal.
  const QPoint mutePoint = muteSwitch->mapToScene(
      QPointF(muteSwitch->width() / 2.0, muteSwitch->height() / 2.0)).toPoint();
  QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, mutePoint);
  QTRY_COMPARE(transport.commits.size(), 1);
  QCOMPARE(transport.commits.first().key, ApplicationPolicies);
  QVERIFY(applicationPolicies.data(applicationPolicies.index(0),
      QindaQt::Apps::SettingsNotifications::NotificationApplicationSettingsModel::MutedRole).toBool());
  QTRY_VERIFY(muteSwitch->property("checked").toBool());
  const auto muteRejected = transport.commits.takeFirst();
  Q_EMIT transport.commitReceived(muteRejected.token, muteRejected.owner,
                                  refusedCommitWire(ApplicationPolicies));
  QTRY_VERIFY(applicationPolicies.errorText().contains(QStringLiteral("blocked")));
  QTRY_COMPARE(transport.snapshots.size(), 1);
  const auto muteReadback = transport.snapshots.takeFirst();
  Q_EMIT transport.snapshotReceived(muteReadback.token, muteReadback.owner,
                                    snapshotWire());
  QTRY_VERIFY(muteSwitch->property("checked").toBool());
  QVERIFY(applicationPolicies.data(applicationPolicies.index(0),
      QindaQt::Apps::SettingsNotifications::NotificationApplicationSettingsModel::MutedRole).toBool());

  // Keyboard activation follows the same confirmed-value rollback contract.
  soundSwitch->forceActiveFocus();
  QTest::keyClick(&view, Qt::Key_Space);
  QTRY_COMPARE(transport.commits.size(), 1);
  QCOMPARE(transport.commits.first().key, ApplicationPolicies);
  QVERIFY(applicationPolicies.data(applicationPolicies.index(0),
      QindaQt::Apps::SettingsNotifications::NotificationApplicationSettingsModel::SoundEnabledRole).toBool());
  QTRY_VERIFY(soundSwitch->property("checked").toBool());
  const auto soundRejected = transport.commits.takeFirst();
  Q_EMIT transport.commitReceived(soundRejected.token, soundRejected.owner,
                                  refusedCommitWire(ApplicationPolicies));
  QTRY_VERIFY(applicationPolicies.errorText().contains(QStringLiteral("blocked")));
  QTRY_COMPARE(transport.snapshots.size(), 1);
  const auto soundReadback = transport.snapshots.takeFirst();
  Q_EMIT transport.snapshotReceived(soundReadback.token, soundReadback.owner,
                                    snapshotWire());
  QTRY_VERIFY(soundSwitch->property("checked").toBool());
  QVERIFY(applicationPolicies.data(applicationPolicies.index(0),
      QindaQt::Apps::SettingsNotifications::NotificationApplicationSettingsModel::SoundEnabledRole).toBool());

  dndSwitch->forceActiveFocus();
  QTest::keyClick(&view, Qt::Key_Space);
  QTRY_COMPARE(transport.commits.size(), 1);
  QTRY_VERIFY(!dndSwitch->property("checked").toBool());
  QVERIFY(!dnd.enabled());
  const auto rejected = transport.commits.takeFirst();
  Q_EMIT transport.commitReceived(rejected.token, rejected.owner, refusedCommitWire(Dnd));
  QTRY_VERIFY(dnd.errorText().contains(QStringLiteral("blocked")));
  QTRY_COMPARE(transport.snapshots.size(), 1);
  const auto readback = transport.snapshots.takeFirst();
  Q_EMIT transport.snapshotReceived(readback.token, readback.owner, snapshotWire());
  QVERIFY(!dndSwitch->property("checked").toBool());
  QVERIFY(!dnd.enabled());
  QVERIFY(dnd.errorText().contains(QStringLiteral("blocked")));
  QVERIFY(transport.commits.isEmpty());

  // A lost commit reply is uncertain, never an optimistic mute or sound
  // policy. The control stays on its last confirmed value during bus loss.
  soundSwitch->forceActiveFocus();
  QTest::keyClick(&view, Qt::Key_Space);
  QTRY_COMPARE(transport.commits.size(), 1);
  QTRY_VERIFY(soundSwitch->property("checked").toBool());
  Q_EMIT transport.busDisconnected();
  QTRY_VERIFY(applicationPolicies.uncertain());
  QVERIFY(!applicationPolicies.pending());
  QVERIFY(applicationPolicies.data(applicationPolicies.index(0),
      QindaQt::Apps::SettingsNotifications::NotificationApplicationSettingsModel::SoundEnabledRole).toBool());
  QTRY_VERIFY(soundSwitch->property("checked").toBool());
}

QTEST_MAIN(NotificationPageAdmissionTest)
#include "tst_notification_page_admission.moc"
