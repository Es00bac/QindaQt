// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include <QtDBus/QDBusMetaType>

#include <algorithm>

namespace QindaQt::Network::SecretAgent {
namespace {

void wipeByteArray(QByteArray &bytes) noexcept {
  if (!bytes.isEmpty()) {
    bytes.detach();
    std::fill(bytes.begin(), bytes.end(), '\0');
    bytes.clear();
  }
}

void wipeVariant(QVariant &value) noexcept {
  if (value.metaType() == QMetaType::fromType<QString>()) {
    QString text = value.toString();
    text.fill(QChar::Null);
    text.clear();
    value.clear();
  } else if (value.metaType() == QMetaType::fromType<QByteArray>()) {
    QByteArray bytes = value.toByteArray();
    wipeByteArray(bytes);
    value.clear();
  }
}

} // namespace

void SecretValue::wipe() noexcept { wipeByteArray(bytes); }

void PromptResult::wipe() noexcept {
  for (SecretValue &value : values) {
    value.wipe();
  }
  values.clear();
}

void SecretReply::wipe() noexcept {
  for (SecretValue &value : values) {
    value.wipe();
  }
  values.clear();
}

void registerSecretAgentDBusTypes() {
  qRegisterMetaType<NmSettingsMap>();
  qDBusRegisterMetaType<NmSettingsMap>();
}

void wipeSettingsMap(NmSettingsMap &settings) noexcept {
  for (QVariantMap &section : settings) {
    for (QVariant &value : section) {
      wipeVariant(value);
    }
    section.clear();
  }
  settings.clear();
}

} // namespace QindaQt::Network::SecretAgent
