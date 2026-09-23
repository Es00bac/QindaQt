// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/apps/settings/notifications/notification_schedule_model.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
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
const QString Owner = QStringLiteral(":1.71");
const QString Epoch = QStringLiteral("notification-epoch-a");

class FakeTransport final : public SettingsTransport {
public:
  struct Request { quint64 token; QString owner; };
  bool start(QString *) override { return true; }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &) override {
    snapshots.append({token, owner});
  }
  void commit(quint64 token, const QString &owner, const QString &, quint64,
              const QVariantList &) override {
    commits.append({token, owner});
  }
  void requestActivation() override {}
  QList<Request> snapshots;
  QList<Request> commits;
};

QVariantMap values() {
  return {{Dnd, false}, {Schedule, false}, {Start, 1320}, {End, 420}};
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

QVariantMap refusedCommitWire() {
  const QVariantMap confirmed{{Dnd, false}};
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::ReadOnlyLayer)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion), WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), Epoch},
          {QLatin1StringView(WireContract::FieldRevisionBefore), quint64(1)},
          {QLatin1StringView(WireContract::FieldRevisionAfter), quint64(1)},
          {QLatin1StringView(WireContract::FieldValues), confirmed},
          {QLatin1StringView(WireContract::FieldSourceLayers), sources(confirmed)},
          {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{}},
          {QLatin1StringView(WireContract::FieldMessage),
           QStringLiteral("DND policy blocked this change")}};
}
} // namespace

class NotificationPageAdmissionTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void refreshAndRefusalKeepBothSwitchesAuthoritative();
};

void NotificationPageAdmissionTest::refreshAndRefusalKeepBothSwitchesAuthoritative() {
  FakeTransport transport;
  SettingsClient client(transport, {Dnd, Schedule, Start, End},
                        {.requestTimeoutMilliseconds = 500,
                         .debounceMilliseconds = 0,
                         .retryMilliseconds = {10}});
  NotificationScheduleModel schedule(client);
  DoNotDisturbController dnd(client);
  QVERIFY(client.start());
  Q_EMIT transport.ownerChanged(Owner);
  QTRY_COMPARE(transport.snapshots.size(), 1);
  const auto first = transport.snapshots.takeFirst();
  Q_EMIT transport.snapshotReceived(first.token, first.owner, snapshotWire());
  QVERIFY(dnd.ready());
  QVERIFY(schedule.canEdit());

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
      {QStringLiteral("quietingSchedule"), QVariant::fromValue(static_cast<QObject *>(&schedule))}});
  view.setSource(QUrl::fromLocalFile(
      QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR "/NotificationsPage.qml")));
  QVERIFY2(view.status() == QQuickView::Ready, qPrintable(view.errors().isEmpty()
      ? QString{} : view.errors().constFirst().toString()));
  view.resize(600, 500);
  view.show();
  QVERIFY(QTest::qWaitForWindowExposed(&view));
  auto *dndSwitch = view.rootObject()->findChild<QQuickItem *>(
      QStringLiteral("settingsDoNotDisturbSwitch"));
  auto *scheduleSwitch = view.rootObject()->findChild<QQuickItem *>(
      QStringLiteral("settingsQuietHoursSwitch"));
  QVERIFY(dndSwitch != nullptr);
  QVERIFY(scheduleSwitch != nullptr);
  QVERIFY(dndSwitch->isEnabled());
  QVERIFY(scheduleSwitch->isEnabled());
  QVERIFY(!dndSwitch->property("checked").toBool());
  QVERIFY(!scheduleSwitch->property("checked").toBool());

  client.refresh();
  QTRY_COMPARE(transport.snapshots.size(), 1);
  // The client's public state remains Ready while its serial request lane is
  // occupied. The actual route must disable both controls immediately.
  QVERIFY(!dndSwitch->isEnabled());
  QVERIFY(!scheduleSwitch->isEnabled());
  dndSwitch->forceActiveFocus();
  QTest::keyClick(&view, Qt::Key_Space);
  QCoreApplication::processEvents();
  QVERIFY(transport.commits.isEmpty());
  QVERIFY(!dndSwitch->property("checked").toBool());

  const auto refresh = transport.snapshots.takeFirst();
  Q_EMIT transport.snapshotReceived(refresh.token, refresh.owner, snapshotWire());
  QTRY_VERIFY(dndSwitch->isEnabled());
  QTRY_VERIFY(scheduleSwitch->isEnabled());
  QVERIFY(!dndSwitch->property("checked").toBool());
  QVERIFY(!scheduleSwitch->property("checked").toBool());

  dndSwitch->forceActiveFocus();
  QTest::keyClick(&view, Qt::Key_Space);
  QTRY_COMPARE(transport.commits.size(), 1);
  QTRY_VERIFY(!dndSwitch->property("checked").toBool());
  QVERIFY(!dnd.enabled());
  const auto rejected = transport.commits.takeFirst();
  Q_EMIT transport.commitReceived(rejected.token, rejected.owner, refusedCommitWire());
  QTRY_VERIFY(dnd.errorText().contains(QStringLiteral("blocked")));
  QTRY_COMPARE(transport.snapshots.size(), 1);
  const auto readback = transport.snapshots.takeFirst();
  Q_EMIT transport.snapshotReceived(readback.token, readback.owner, snapshotWire());
  QVERIFY(!dndSwitch->property("checked").toBool());
  QVERIFY(!dnd.enabled());
  QVERIFY(dnd.errorText().contains(QStringLiteral("blocked")));
  QVERIFY(transport.commits.isEmpty());
}

QTEST_MAIN(NotificationPageAdmissionTest)
#include "tst_notification_page_admission.moc"
