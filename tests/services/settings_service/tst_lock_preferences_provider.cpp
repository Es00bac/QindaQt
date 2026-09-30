// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/lock_preferences/lock_preferences.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include <QtTest>
using namespace QindaQt::Services::LockPreferences;
using namespace QindaQt::Services::SettingsClient;
using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
using QindaQt::Services::SettingsProtocol::WireContract;
class PreferenceTransport final : public SettingsTransport {
  Q_OBJECT
public:
  bool start(QString *) override { return true; }
  void stop() override {}
  void requestActivation() override {}
  void requestSnapshot(quint64 token, const QString &owner,
                       const QStringList &) override {
    requests.append({token, owner});
  }
  void commit(quint64, const QString &, const QString &, quint64,
              const QVariantList &) override {}
  struct Request {
    quint64 token;
    QString owner;
  };
  QList<Request> requests;
};
namespace {
QVariantMap wire(const QString &epoch, const QVariantMap &values) {
  QVariantMap sources;
  for (auto it = values.cbegin(); it != values.cend(); ++it)
    sources.insert(it.key(), QStringLiteral("user-overrides"));
  return {{"status", quint32(SettingsWireStatus::Applied)},
          {"wireSchemaVersion", WireContract::WireSchemaVersion},
          {"settingsSchemaVersion", quint32(2)},
          {"epoch", epoch},
          {"revision", quint64(0)},
          {"values", values},
          {"sourceLayers", sources},
          {"message", QString{}}};
}
const QVariantMap values{{"lock.automaticEnabled", true},
                         {"lock.idleTimeoutSeconds", qint64(300)},
                         {"lock.onResume", true},
                         {"lock.graceSeconds", qint64(5)}};
} // namespace
class LockPreferencesProviderTests final : public QObject {
  Q_OBJECT
private slots:
  void ownerReplacementLossAndMalformedDataRevokePreferences();
};
void LockPreferencesProviderTests::
    ownerReplacementLossAndMalformedDataRevokePreferences() {
  PreferenceTransport transport;
  SettingsClient client(transport, scopedKeys(),
                        {.requestTimeoutMilliseconds = 200,
                         .debounceMilliseconds = 0,
                         .retryMilliseconds = {10}});
  PreferencesProvider provider(client);
  QSignalSpy changed(&provider, &PreferencesProvider::changed);
  QVERIFY(!provider.preferences());
  QVERIFY(client.start());
  Q_EMIT transport.ownerChanged(":1.10");
  QTRY_COMPARE(transport.requests.size(), 1);
  const auto first = transport.requests.takeFirst();
  Q_EMIT transport.snapshotReceived(first.token, first.owner,
                                    wire("first-epoch", values));
  QTRY_VERIFY(client.state() == ClientState::Ready);
  QVERIFY(provider.preferences());
  QCOMPARE(*provider.preferences(), Preferences{});
  QVERIFY(!changed.isEmpty());
  Q_EMIT transport.ownerChanged(":1.11");
  QVERIFY(!provider.preferences());
  QTRY_COMPARE(transport.requests.size(), 1);
  const auto next = transport.requests.takeFirst();
  Q_EMIT transport.snapshotReceived(first.token, first.owner,
                                    wire("first-epoch", values));
  QVERIFY(!provider.preferences());
  auto malformed = values;
  malformed.insert("lock.onResume", QStringLiteral("true"));
  Q_EMIT transport.snapshotReceived(next.token, next.owner,
                                    wire("next-epoch", malformed));
  QTRY_VERIFY(client.state() == ClientState::Ready);
  QVERIFY(!provider.preferences());
  Q_EMIT transport.ownerChanged(":1.12");
  QTRY_COMPARE(transport.requests.size(), 1);
  const auto valid = transport.requests.takeFirst();
  Q_EMIT transport.snapshotReceived(valid.token, valid.owner,
                                    wire("valid-epoch", values));
  QTRY_VERIFY(provider.preferences());
  Q_EMIT transport.busDisconnected();
  QVERIFY(!provider.preferences());
  client.stop();
  QVERIFY(!provider.preferences());
}
QTEST_GUILESS_MAIN(LockPreferencesProviderTests)
#include "tst_lock_preferences_provider.moc"
