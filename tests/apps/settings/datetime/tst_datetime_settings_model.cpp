// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_datetime/datetime_settings_model.h"

#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::SettingsDateTime;

namespace {

// Records what the route asked of the platform and answers only when the test
// says so, so every refusal, delay and read-back is observable without a bus,
// a systemd, or a polkit agent.
class FakeTimeService final : public SystemTimeService {
public:
  void refresh() override { ++m_refreshes; }
  void setTimeZone(const QString &timeZone) override {
    m_requestedTimeZones.append(timeZone);
  }
  void setAutomaticTime(bool enabled) override {
    m_requestedAutomatic.append(enabled);
  }

  void publish(const SystemTimeSnapshot &snapshot) {
    Q_EMIT snapshotChanged(snapshot);
  }
  void refuse(const QString &diagnostic) { Q_EMIT requestFailed(diagnostic); }

  int m_refreshes = 0;
  QStringList m_requestedTimeZones;
  QList<bool> m_requestedAutomatic;
};

class FakeWeekStart final : public WeekStartPreference {
public:
  [[nodiscard]] QString weekStart() const override { return m_weekStart; }
  [[nodiscard]] bool editable() const override { return m_editable; }
  [[nodiscard]] WeekStartWriteState writeState() const override { return m_state; }
  [[nodiscard]] QString diagnostic() const override { return m_diagnostic; }
  [[nodiscard]] QString availabilityText() const override {
    return m_editable ? QString{} : QStringLiteral("Settings unavailable");
  }
  bool setWeekStart(const QString &weekStart) override {
    m_requested.append(weekStart);
    if (!m_accept) {
      m_state = WeekStartWriteState::Refused;
      m_diagnostic = QStringLiteral("Write refused");
      Q_EMIT weekStartChanged();
      return false;
    }
    m_state = WeekStartWriteState::Idle;
    m_diagnostic.clear();
    m_weekStart = weekStart;
    Q_EMIT weekStartChanged();
    return true;
  }
  void refresh() override {
    ++m_refreshes;
    Q_EMIT weekStartChanged();
  }

  QString m_weekStart = QStringLiteral("locale");
  bool m_editable = true;
  bool m_accept = true;
  QStringList m_requested;
  WeekStartWriteState m_state = WeekStartWriteState::Idle;
  QString m_diagnostic;
  int m_refreshes = 0;
};

[[nodiscard]] SystemTimeSnapshot readySnapshot() {
  SystemTimeSnapshot snapshot;
  snapshot.available = true;
  snapshot.timeZone = QStringLiteral("America/Denver");
  snapshot.timeZones = {QStringLiteral("America/Denver"),
                        QStringLiteral("Europe/Berlin"), QStringLiteral("UTC")};
  snapshot.automaticTime = true;
  snapshot.automaticTimeSupported = true;
  snapshot.synchronized = true;
  snapshot.locale = QStringLiteral("en_US.UTF-8");
  return snapshot;
}

struct Fixture final {
  FakeTimeService *service = nullptr;
  FakeWeekStart *week = nullptr;
  std::unique_ptr<DateTimeSettingsModel> model;

  Fixture() {
    auto ownedService = std::make_unique<FakeTimeService>();
    auto ownedWeek = std::make_unique<FakeWeekStart>();
    service = ownedService.get();
    week = ownedWeek.get();
    model = std::make_unique<DateTimeSettingsModel>(std::move(ownedService),
                                                    std::move(ownedWeek));
  }
};

} // namespace

class TestDateTimeSettingsModel final : public QObject {
  Q_OBJECT

private slots:
  void asksForASnapshotOnceAtStartup();
  void publishesWhatThePlatformSaid();
  void anUnavailableServiceSaysSoAndEditsNothing();
  void requestsOnlyARealChangeAndOnlyAnOfferedZone();
  void aRefusalLeavesEveryValueAsItWas();
  void automaticTimeIsUnavailableWithoutSupport();
  void theZonePickerIsReadOnlyWithoutACatalogue();
  void firstDayOfTheWeekFollowsThePreference();
};

void TestDateTimeSettingsModel::asksForASnapshotOnceAtStartup() {
  Fixture fixture;
  QCOMPARE(fixture.service->m_refreshes, 1);
  // Before the platform answers, the page is unavailable rather than showing
  // empty fields that look like real values.
  QVERIFY(!fixture.model->available());
  QVERIFY(fixture.model->timeZone().isEmpty());
  QVERIFY(!fixture.model->statusText().isEmpty());

  fixture.model->refresh();
  QCOMPARE(fixture.service->m_refreshes, 2);
}

void TestDateTimeSettingsModel::publishesWhatThePlatformSaid() {
  Fixture fixture;
  QSignalSpy changed(fixture.model.get(), &DateTimeSettingsModel::viewChanged);
  fixture.service->publish(readySnapshot());

  QCOMPARE(changed.count(), 1);
  QVERIFY(fixture.model->available());
  QCOMPARE(fixture.model->timeZone(), QStringLiteral("America/Denver"));
  QCOMPARE(fixture.model->timeZones().size(), 3);
  QVERIFY(fixture.model->timeZoneEditable());
  QVERIFY(fixture.model->automaticTime());
  QVERIFY(fixture.model->automaticTimeSupported());
  QCOMPARE(fixture.model->locale(), QStringLiteral("en_US.UTF-8"));
  QVERIFY(fixture.model->synchronizationText().contains(
      QStringLiteral("synchronized")));
  QVERIFY(fixture.model->statusText().isEmpty());
  QVERIFY(!fixture.model->busy());

  // An identical snapshot is not republished.
  fixture.service->publish(readySnapshot());
  QCOMPARE(changed.count(), 1);
}

void TestDateTimeSettingsModel::anUnavailableServiceSaysSoAndEditsNothing() {
  Fixture fixture;
  fixture.service->publish({});
  QVERIFY(!fixture.model->available());
  QVERIFY(!fixture.model->timeZoneEditable());
  QVERIFY(!fixture.model->automaticTimeSupported());
  QVERIFY(fixture.model->synchronizationText().isEmpty());
  QVERIFY(fixture.model->statusText().contains(QStringLiteral("unavailable")));

  fixture.model->requestTimeZone(QStringLiteral("UTC"));
  fixture.model->requestAutomaticTime(true);
  QVERIFY(fixture.service->m_requestedTimeZones.isEmpty());
  QVERIFY(fixture.service->m_requestedAutomatic.isEmpty());
}

void TestDateTimeSettingsModel::requestsOnlyARealChangeAndOnlyAnOfferedZone() {
  Fixture fixture;
  fixture.service->publish(readySnapshot());

  // The zone that is already set is not requested again.
  fixture.model->requestTimeZone(QStringLiteral("America/Denver"));
  QVERIFY(fixture.service->m_requestedTimeZones.isEmpty());
  // AGENT-GUARD: only a zone the platform itself offered is ever requested.
  fixture.model->requestTimeZone(QStringLiteral("Mars/Olympus"));
  QVERIFY(fixture.service->m_requestedTimeZones.isEmpty());

  fixture.model->requestTimeZone(QStringLiteral("Europe/Berlin"));
  QCOMPARE(fixture.service->m_requestedTimeZones,
           QStringList{QStringLiteral("Europe/Berlin")});
  QVERIFY(fixture.model->busy());
  // The published value is still the old one: nothing moves until the platform
  // confirms it.
  QCOMPARE(fixture.model->timeZone(), QStringLiteral("America/Denver"));

  SystemTimeSnapshot moved = readySnapshot();
  moved.timeZone = QStringLiteral("Europe/Berlin");
  fixture.service->publish(moved);
  QCOMPARE(fixture.model->timeZone(), QStringLiteral("Europe/Berlin"));
  QVERIFY(!fixture.model->busy());

  fixture.model->requestAutomaticTime(true);
  QVERIFY(fixture.service->m_requestedAutomatic.isEmpty());
  fixture.model->requestAutomaticTime(false);
  QCOMPARE(fixture.service->m_requestedAutomatic, QList<bool>{false});
}

void TestDateTimeSettingsModel::aRefusalLeavesEveryValueAsItWas() {
  Fixture fixture;
  fixture.service->publish(readySnapshot());
  fixture.model->requestTimeZone(QStringLiteral("UTC"));
  QVERIFY(fixture.model->busy());

  // AGENT-GUARD: polkit saying no must not look like success.
  fixture.service->refuse(QStringLiteral("Interactive authentication required."));
  QCOMPARE(fixture.model->timeZone(), QStringLiteral("America/Denver"));
  QVERIFY(!fixture.model->busy());
  QCOMPARE(fixture.model->errorText(),
           QStringLiteral("Interactive authentication required."));

  // A new request clears the old complaint.
  fixture.model->requestTimeZone(QStringLiteral("UTC"));
  QVERIFY(fixture.model->errorText().isEmpty());
  fixture.service->refuse(QStringLiteral("again"));
  fixture.model->clearError();
  QVERIFY(fixture.model->errorText().isEmpty());
}

void TestDateTimeSettingsModel::automaticTimeIsUnavailableWithoutSupport() {
  Fixture fixture;
  SystemTimeSnapshot noNtp = readySnapshot();
  noNtp.automaticTimeSupported = false;
  noNtp.automaticTime = false;
  noNtp.synchronized = false;
  fixture.service->publish(noNtp);

  // A machine with no time-synchronization service shows the control
  // unavailable rather than merely off.
  QVERIFY(!fixture.model->automaticTimeSupported());
  QCOMPARE(fixture.model->synchronizationText(),
           QStringLiteral("The clock is set manually."));
  fixture.model->requestAutomaticTime(true);
  QVERIFY(fixture.service->m_requestedAutomatic.isEmpty());

  SystemTimeSnapshot waiting = readySnapshot();
  waiting.synchronized = false;
  fixture.service->publish(waiting);
  QCOMPARE(fixture.model->synchronizationText(),
           QStringLiteral("Waiting for a time server."));
}

void TestDateTimeSettingsModel::theZonePickerIsReadOnlyWithoutACatalogue() {
  Fixture fixture;
  SystemTimeSnapshot noCatalogue = readySnapshot();
  noCatalogue.timeZones.clear();
  fixture.service->publish(noCatalogue);

  // The current zone is still shown; only the picker is closed, because an
  // empty picker looks broken.
  QCOMPARE(fixture.model->timeZone(), QStringLiteral("America/Denver"));
  QVERIFY(!fixture.model->timeZoneEditable());
  fixture.model->requestTimeZone(QStringLiteral("UTC"));
  QVERIFY(fixture.service->m_requestedTimeZones.isEmpty());
}

void TestDateTimeSettingsModel::firstDayOfTheWeekFollowsThePreference() {
  Fixture fixture;
  QCOMPARE(fixture.model->weekStart(), QStringLiteral("locale"));
  QCOMPARE(fixture.model->weekStarts(),
           QStringList({QStringLiteral("locale"), QStringLiteral("monday"),
                        QStringLiteral("sunday")}));
  QVERIFY(fixture.model->weekStartEditable());

  fixture.model->requestWeekStart(QStringLiteral("monday"));
  QCOMPARE(fixture.week->m_requested, QStringList{QStringLiteral("monday")});
  QCOMPARE(fixture.model->weekStart(), QStringLiteral("monday"));
  // The same value again is not written.
  fixture.model->requestWeekStart(QStringLiteral("monday"));
  QCOMPARE(fixture.week->m_requested.size(), 1);
  // Nor is a value outside the set.
  fixture.model->requestWeekStart(QStringLiteral("caturday"));
  QCOMPARE(fixture.week->m_requested.size(), 1);

  // A settings service that is not ready leaves the control unavailable, and a
  // refused write leaves the published value alone.
  fixture.week->m_editable = false;
  QVERIFY(!fixture.model->weekStartEditable());
  fixture.model->requestWeekStart(QStringLiteral("sunday"));
  QCOMPARE(fixture.week->m_requested.size(), 1);
  fixture.week->m_editable = true;
  fixture.week->m_accept = false;
  fixture.model->requestWeekStart(QStringLiteral("sunday"));
  QCOMPARE(fixture.week->m_requested.size(), 2);
  QCOMPARE(fixture.model->weekStart(), QStringLiteral("monday"));
  QVERIFY(fixture.model->weekStartErrorText().contains(QStringLiteral("refused")));
  QVERIFY(!fixture.model->weekStartPending());
  fixture.model->retryWeekStart();
  QCOMPARE(fixture.week->m_refreshes, 1);
}

QTEST_MAIN(TestDateTimeSettingsModel)
#include "tst_datetime_settings_model.moc"
