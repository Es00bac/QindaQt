// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0271: the selected desktop layout's File Manager style -- the stock
// profiles' workflow.fileManager hints, and Settings1's panels.layoutProfile
// followed live through a scripted transport. No bus is reached.
#include "runtime/layout_style_hint.h"

#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace QindaQt::Apps::FileManager;
using QindaQt::Services::SettingsClient::SettingsTransport;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;

namespace {

const QString kLayoutKey = QStringLiteral("panels.layoutProfile");

[[nodiscard]] QString stockProfiles() {
  return QStringLiteral(QINDAQT_SOURCE_DIR) + QStringLiteral("/data/profiles");
}

class ScriptedTransport final : public SettingsTransport {
  Q_OBJECT
public:
  struct Request {
    quint64 token;
    QString owner;
  };
  QList<Request> snapshots;

  bool start(QString *error) override {
    if (error)
      error->clear();
    return true;
  }
  void stop() override {}
  void requestSnapshot(quint64 token, const QString &owner, const QStringList &) override {
    snapshots.append({token, owner});
  }
  void commit(quint64, const QString &, const QString &, quint64, const QVariantList &) override {}
  void requestActivation() override {}
};

QVariantMap snapshotWire(quint64 revision, const QString &layout) {
  return {{QLatin1StringView(WireContract::FieldStatus), quint32(SettingsWireStatus::Applied)},
          {QLatin1StringView(WireContract::FieldWireSchemaVersion),
           WireContract::WireSchemaVersion},
          {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
          {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch")},
          {QLatin1StringView(WireContract::FieldRevision), revision},
          {QLatin1StringView(WireContract::FieldValues), QVariantMap{{kLayoutKey, layout}}},
          {QLatin1StringView(WireContract::FieldSourceLayers),
           QVariantMap{{kLayoutKey, QStringLiteral("user-overrides")}}},
          {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

} // namespace

class TestLayoutStyleHint final : public QObject {
  Q_OBJECT

private slots:
  void readsEveryStockLayoutsHint();
  void aLaterDirectoryOverridesAnEarlierOne();
  void followsTheSelectedLayout();
};

void TestLayoutStyleHint::readsEveryStockLayoutsHint() {
  const auto hints = LayoutStyleHint::loadProfileHints({stockProfiles()});
  // ADR-0268's pairing: the Mac-like layouts are Finder, the Windows-like
  // ones (Windows 3.1 included: a tree beside the list) Explorer.
  QCOMPARE(hints.value(QStringLiteral("macos-inspired")), QStringLiteral("finder"));
  QCOMPARE(hints.value(QStringLiteral("windows-modern")), QStringLiteral("explorer"));
  QCOMPARE(hints.value(QStringLiteral("win31-inspired")), QStringLiteral("explorer"));
  for (auto hint = hints.cbegin(); hint != hints.cend(); ++hint) {
    QVERIFY2(QStringList({QStringLiteral("finder"), QStringLiteral("explorer"),
                          QStringLiteral("commander")})
                 .contains(hint.value()),
             qPrintable(hint.key()));
  }
}

void TestLayoutStyleHint::aLaterDirectoryOverridesAnEarlierOne() {
  QTemporaryDir user;
  QVERIFY(user.isValid());
  QFile stock(stockProfiles() + QStringLiteral("/macos-inspired.json"));
  QVERIFY(stock.open(QIODevice::ReadOnly));
  QByteArray json = stock.readAll();
  QVERIFY(json.contains("\"fileManager\": \"finder\""));
  json.replace("\"fileManager\": \"finder\"", "\"fileManager\": \"commander\"");
  QFile copy(user.filePath(QStringLiteral("macos-inspired.json")));
  QVERIFY(copy.open(QIODevice::WriteOnly));
  QCOMPARE(copy.write(json), json.size());
  copy.close();
  // A file that does not load gives no hint, and takes nothing else down.
  QFile broken(user.filePath(QStringLiteral("broken.json")));
  QVERIFY(broken.open(QIODevice::WriteOnly));
  QVERIFY(broken.write("{") == 1);
  broken.close();

  const auto hints = LayoutStyleHint::loadProfileHints({stockProfiles(), user.path()});
  QCOMPARE(hints.value(QStringLiteral("macos-inspired")), QStringLiteral("commander"));
  QCOMPARE(hints.value(QStringLiteral("windows-modern")), QStringLiteral("explorer"));
}

void TestLayoutStyleHint::followsTheSelectedLayout() {
  auto owned = std::make_unique<ScriptedTransport>();
  ScriptedTransport *transport = owned.get();
  LayoutStyleHint hint(std::move(owned), {{QStringLiteral("windows-modern"), QStringLiteral("explorer")},
                                          {QStringLiteral("macos-inspired"), QStringLiteral("finder")}});
  QSignalSpy changed(&hint, &LayoutStyleHint::hintChanged);
  // Nothing is known before Settings1 answers.
  QVERIFY(hint.hint().isEmpty());

  Q_EMIT transport->ownerChanged(QStringLiteral(":1.40"));
  QTRY_COMPARE(transport->snapshots.size(), 1);
  auto request = transport->snapshots.takeFirst();
  Q_EMIT transport->snapshotReceived(request.token, request.owner,
                                     snapshotWire(1, QStringLiteral("windows-modern")));
  QTRY_COMPARE(hint.hint(), QStringLiteral("explorer"));
  QCOMPARE(changed.count(), 1);
  QCOMPARE(changed.constLast().constFirst().toString(), QStringLiteral("explorer"));

  // The user picks another layout in Settings.
  Q_EMIT transport->settingsChanged(QStringLiteral(":1.40"), QStringLiteral("epoch"), 2,
                                QStringList{kLayoutKey});
  QTRY_COMPARE(transport->snapshots.size(), 1);
  request = transport->snapshots.takeFirst();
  Q_EMIT transport->snapshotReceived(request.token, request.owner,
                                     snapshotWire(2, QStringLiteral("macos-inspired")));
  QTRY_COMPARE(hint.hint(), QStringLiteral("finder"));

  // A layout the catalog does not know gives no hint (Finder).
  Q_EMIT transport->settingsChanged(QStringLiteral(":1.40"), QStringLiteral("epoch"), 3,
                                QStringList{kLayoutKey});
  QTRY_COMPARE(transport->snapshots.size(), 1);
  request = transport->snapshots.takeFirst();
  Q_EMIT transport->snapshotReceived(request.token, request.owner,
                                     snapshotWire(3, QStringLiteral("someone-elses")));
  QTRY_VERIFY(hint.hint().isEmpty());
  QCOMPARE(changed.count(), 3);
}

QTEST_GUILESS_MAIN(TestLayoutStyleHint)
#include "tst_layout_style_hint.moc"
