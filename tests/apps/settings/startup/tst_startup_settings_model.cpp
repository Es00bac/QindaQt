// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_startup/startup_settings_model.h>

#include <QTest>

using namespace QindaQt::Apps::SettingsStartup;

namespace {

class FakeAutostartStore final : public AutostartStore {
public:
  QList<AutostartEntry> entries;
  QString nextError;
  int listCalls = 0;
  int setEnabledCalls = 0;

  [[nodiscard]] QList<AutostartEntry> list(QString *error) override {
    ++listCalls;
    if (error != nullptr) {
      error->clear();
    }
    return entries;
  }

  [[nodiscard]] bool setEnabled(const QString &id, const bool enabled,
                                QString *error) override {
    ++setEnabledCalls;
    if (!nextError.isEmpty()) {
      if (error != nullptr) {
        *error = nextError;
      }
      nextError.clear();
      return false;
    }
    for (AutostartEntry &entry : entries) {
      if (entry.id == id) {
        entry.enabled = enabled;
        return true;
      }
    }
    if (error != nullptr) {
      *error = QStringLiteral("unknown id");
    }
    return false;
  }

  [[nodiscard]] QString addCommand(const QString &name, const QString &command,
                                   QString *error) override {
    if (!nextError.isEmpty()) {
      if (error != nullptr) {
        *error = nextError;
      }
      nextError.clear();
      return {};
    }
    AutostartEntry entry;
    entry.id = QStringLiteral("qindaqt-custom-%1").arg(entries.size());
    entry.name = name;
    entry.exec = command;
    entry.enabled = true;
    entry.custom = true;
    entries.append(entry);
    return entry.id;
  }

  [[nodiscard]] bool removeCustom(const QString &id, QString *error) override {
    if (!nextError.isEmpty()) {
      if (error != nullptr) {
        *error = nextError;
      }
      nextError.clear();
      return false;
    }
    for (qsizetype index = 0; index < entries.size(); ++index) {
      if (entries.at(index).id == id && entries.at(index).custom) {
        entries.removeAt(index);
        return true;
      }
    }
    if (error != nullptr) {
      *error = QStringLiteral("not custom");
    }
    return false;
  }
};

} // namespace

class StartupSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void projectsEntriesOnConstruction();
  void setEnabledRefreshesOnSuccess();
  void setEnabledReportsErrorAndKeepsPreviousListOnFailure();
  void addCommandRefreshesOnSuccess();
  void addCommandReportsErrorOnFailure();
  void removeCustomRefreshesOnSuccess();
};

void StartupSettingsModelTest::projectsEntriesOnConstruction() {
  auto store = std::make_unique<FakeAutostartStore>();
  FakeAutostartStore *raw = store.get();
  raw->entries.append(
      AutostartEntry{.id = QStringLiteral("a"), .name = QStringLiteral("A"),
                     .comment = QStringLiteral("comment"),
                     .iconName = QStringLiteral("icon"),
                     .exec = QStringLiteral("a --run"), .enabled = true,
                     .eligible = true,
                     .ineligibilityReason = {}, .custom = false});
  StartupSettingsModel model(std::move(store));
  QCOMPARE(raw->listCalls, 1);
  const QVariantList entries = model.entries();
  QCOMPARE(entries.size(), 1);
  const QVariantMap row = entries.constFirst().toMap();
  QCOMPARE(row.value(QStringLiteral("id")).toString(), QStringLiteral("a"));
  QCOMPARE(row.value(QStringLiteral("name")).toString(), QStringLiteral("A"));
  QCOMPARE(row.value(QStringLiteral("comment")).toString(),
           QStringLiteral("comment"));
  QCOMPARE(row.value(QStringLiteral("exec")).toString(),
           QStringLiteral("a --run"));
  QVERIFY(row.value(QStringLiteral("enabled")).toBool());
  QVERIFY(row.value(QStringLiteral("eligible")).toBool());
  QVERIFY(row.value(QStringLiteral("ineligibilityReason")).toString().isEmpty());
  QVERIFY(!row.value(QStringLiteral("custom")).toBool());
  QVERIFY(model.errorText().isEmpty());
}

void StartupSettingsModelTest::setEnabledRefreshesOnSuccess() {
  auto store = std::make_unique<FakeAutostartStore>();
  FakeAutostartStore *raw = store.get();
  raw->entries.append(AutostartEntry{.id = QStringLiteral("a"),
                                     .name = QStringLiteral("A"),
                                     .comment = QString(),
                                     .iconName = QString(),
                                     .exec = QString(),
                                     .enabled = true,
                                     .eligible = true,
                     .ineligibilityReason = {}, .custom = false});
  StartupSettingsModel model(std::move(store));
  QVERIFY(model.setEnabled(QStringLiteral("a"), false));
  QCOMPARE(raw->setEnabledCalls, 1);
  QCOMPARE(raw->listCalls, 2);
  QVERIFY(!model.entries().constFirst().toMap()
               .value(QStringLiteral("enabled")).toBool());
}

void StartupSettingsModelTest::
    setEnabledReportsErrorAndKeepsPreviousListOnFailure() {
  auto store = std::make_unique<FakeAutostartStore>();
  FakeAutostartStore *raw = store.get();
  raw->entries.append(AutostartEntry{.id = QStringLiteral("a"),
                                     .name = QStringLiteral("A"),
                                     .comment = QString(),
                                     .iconName = QString(),
                                     .exec = QString(),
                                     .enabled = true,
                                     .eligible = true,
                     .ineligibilityReason = {}, .custom = false});
  StartupSettingsModel model(std::move(store));
  raw->nextError = QStringLiteral("boom");
  QVERIFY(!model.setEnabled(QStringLiteral("a"), false));
  QCOMPARE(model.errorText(), QStringLiteral("boom"));
  // The list was not re-fetched after a failed mutation; truth still shows
  // the value from before the attempted change.
  QVERIFY(model.entries().constFirst().toMap()
              .value(QStringLiteral("enabled")).toBool());
}

void StartupSettingsModelTest::addCommandRefreshesOnSuccess() {
  auto store = std::make_unique<FakeAutostartStore>();
  StartupSettingsModel model(std::move(store));
  QVERIFY(model.addCommand(QStringLiteral("Sync"), QStringLiteral("sync --daemon")));
  QCOMPARE(model.entries().size(), 1);
  const QVariantMap row = model.entries().constFirst().toMap();
  QCOMPARE(row.value(QStringLiteral("name")).toString(), QStringLiteral("Sync"));
  QVERIFY(row.value(QStringLiteral("custom")).toBool());
}

void StartupSettingsModelTest::addCommandReportsErrorOnFailure() {
  auto store = std::make_unique<FakeAutostartStore>();
  FakeAutostartStore *raw = store.get();
  raw->nextError = QStringLiteral("a name is required");
  StartupSettingsModel model(std::move(store));
  QVERIFY(!model.addCommand(QStringLiteral(""), QStringLiteral("cmd")));
  QCOMPARE(model.errorText(), QStringLiteral("a name is required"));
  QCOMPARE(model.entries().size(), 0);
}

void StartupSettingsModelTest::removeCustomRefreshesOnSuccess() {
  auto store = std::make_unique<FakeAutostartStore>();
  StartupSettingsModel model(std::move(store));
  QVERIFY(model.addCommand(QStringLiteral("Sync"), QStringLiteral("cmd")));
  const QString id = model.entries().constFirst().toMap()
                         .value(QStringLiteral("id")).toString();
  QVERIFY(model.removeCustom(id));
  QCOMPARE(model.entries().size(), 0);
}

QTEST_MAIN(StartupSettingsModelTest)
#include "tst_startup_settings_model.moc"
