// SPDX-License-Identifier: GPL-3.0-or-later

#include "libnm_network_manager_port_p.h"

#include <QtTest>

#include <memory>

using namespace QindaQt::Network;
using namespace QindaQt::Network::NetworkManager;

namespace {

using VariantPtr = std::unique_ptr<GVariant, decltype(&g_variant_unref)>;
using ConnectionPtr =
    std::unique_ptr<NMConnection, decltype(&g_object_unref)>;

VariantPtr setting(GVariant *settings, const char *name) {
  return VariantPtr(
      g_variant_lookup_value(settings, name, G_VARIANT_TYPE("a{sv}")),
      &g_variant_unref);
}

VariantPtr variantProperty(GVariant *settings, const char *name,
                           const GVariantType *type = nullptr) {
  return VariantPtr(g_variant_lookup_value(settings, name, type),
                    &g_variant_unref);
}

} // namespace

class NetworkVisibleProfileTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void buildsSecretFreeProfile_data();
  void buildsSecretFreeProfile();
  void refusesUnsupportedOrHiddenProfiles();
};

void NetworkVisibleProfileTests::buildsSecretFreeProfile_data() {
  QTest::addColumn<SecuritySuite>("security");
  QTest::addColumn<QString>("keyManagement");
  QTest::newRow("open") << SecuritySuite::Open << QString{};
  QTest::newRow("wpa-psk")
      << SecuritySuite::Wpa2Personal << QStringLiteral("wpa-psk");
  QTest::newRow("sae")
      << SecuritySuite::Wpa3Personal << QStringLiteral("sae");
}

void NetworkVisibleProfileTests::buildsSecretFreeProfile() {
  QFETCH(SecuritySuite, security);
  QFETCH(QString, keyManagement);
  ConnectionPtr profile(buildVisibleWifiProfile(QByteArray("New network"),
                                                security),
                        &g_object_unref);
  QVERIFY(profile != nullptr);
  VariantPtr settings(
      nm_connection_to_dbus(profile.get(), NM_CONNECTION_SERIALIZE_ALL),
      &g_variant_unref);
  QVERIFY(settings != nullptr);

  VariantPtr connection =
      setting(settings.get(), NM_SETTING_CONNECTION_SETTING_NAME);
  VariantPtr wireless =
      setting(settings.get(), NM_SETTING_WIRELESS_SETTING_NAME);
  QVERIFY(connection != nullptr);
  QVERIFY(wireless != nullptr);
  QVERIFY(variantProperty(wireless.get(), NM_SETTING_WIRELESS_SSID) != nullptr);

  VariantPtr wifiSecurity =
      setting(settings.get(), NM_SETTING_WIRELESS_SECURITY_SETTING_NAME);
  if (security == SecuritySuite::Open) {
    QVERIFY(wifiSecurity == nullptr);
    return;
  }
  QVERIFY(wifiSecurity != nullptr);
  VariantPtr key = variantProperty(wifiSecurity.get(),
                                   NM_SETTING_WIRELESS_SECURITY_KEY_MGMT,
                                   G_VARIANT_TYPE_STRING);
  QVERIFY(key != nullptr);
  QCOMPARE(QString::fromUtf8(g_variant_get_string(key.get(), nullptr)),
           keyManagement);
  QVERIFY(variantProperty(wifiSecurity.get(), NM_SETTING_WIRELESS_SECURITY_PSK)
              == nullptr);
  VariantPtr flags = variantProperty(wifiSecurity.get(),
                                     NM_SETTING_WIRELESS_SECURITY_PSK_FLAGS,
                                     G_VARIANT_TYPE_UINT32);
  QVERIFY(flags != nullptr);
  QCOMPARE(g_variant_get_uint32(flags.get()),
           static_cast<guint32>(NM_SETTING_SECRET_FLAG_AGENT_OWNED));
}

void NetworkVisibleProfileTests::refusesUnsupportedOrHiddenProfiles() {
  for (const SecuritySuite security : {
           SecuritySuite::Wep, SecuritySuite::Wpa2Enterprise,
           SecuritySuite::Wpa3Enterprise}) {
    QVERIFY(buildVisibleWifiProfile(QByteArray("Unsupported"), security)
            == nullptr);
  }
  QVERIFY(buildVisibleWifiProfile(QByteArray{}, SecuritySuite::Open)
          == nullptr);
  QVERIFY(buildVisibleWifiProfile(QByteArray("\xff\xfe", 2),
                                  SecuritySuite::Wpa2Personal)
          == nullptr);
}

QTEST_GUILESS_MAIN(NetworkVisibleProfileTests)
#include "tst_network_visible_profile.moc"
