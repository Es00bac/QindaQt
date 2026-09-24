// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/network_secret_agent/secret_agent_types.h>

#include "secret_agent_types_p.h"

#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusVariant>

namespace QindaQt::Network::SecretAgent {
namespace {

void overwriteOwnedStorage(void *data, const qsizetype bytes) noexcept {
  auto *volatileData = static_cast<volatile unsigned char *>(data);
  for (qsizetype index = 0; index < bytes; ++index) {
    volatileData[index] = 0U;
  }
}

void wipeByteArray(QByteArray &bytes) noexcept {
  if (!bytes.isEmpty()) {
    // AGENT-GUARD: Do not call non-const data(), detach(), or fill() here.
    // Qt implicit-sharing would preserve the original secret allocation.
    if (bytes.capacity() >= bytes.size()) {
      auto *data = const_cast<char *>(bytes.constData());
      overwriteOwnedStorage(data, bytes.size());
    }
    bytes.clear();
  }
}

void wipeString(QString &text) noexcept {
  if (!text.isEmpty()) {
    // DBus/QML values own dynamic storage. A zero-capacity QString is an
    // external/static view and must not be written through.
    if (text.capacity() >= text.size()) {
      auto *data = const_cast<QChar *>(text.constData());
      overwriteOwnedStorage(data, text.size() * qsizetype(sizeof(QChar)));
    }
    text.clear();
  }
}

void wipeVariant(QVariant &value) noexcept;

template <typename Associative>
void wipeAssociative(Associative &values) noexcept {
  // AGENT-GUARD: Key text shares its backing allocation with the map key. Wipe
  // it only while walking toward an immediate clear; never perform key lookup,
  // insertion, or removal after its ordering/hash text has been overwritten.
  for (auto entry = values.begin(); entry != values.end(); ++entry) {
    QString key = entry.key();
    wipeString(key);
    wipeVariant(entry.value());
  }
  values.clear();
}

void wipeVariant(QVariant &value) noexcept {
  if (value.metaType() == QMetaType::fromType<QString>()) {
    auto *text = static_cast<QString *>(value.data());
    wipeString(*text);
  } else if (value.metaType() == QMetaType::fromType<QByteArray>()) {
    auto *bytes = static_cast<QByteArray *>(value.data());
    wipeByteArray(*bytes);
  } else if (value.metaType() == QMetaType::fromType<QVariantList>()) {
    QVariantList nested = value.toList();
    for (QVariant &entry : nested) {
      wipeVariant(entry);
    }
    nested.clear();
  } else if (value.metaType() == QMetaType::fromType<QVariantMap>()) {
    QVariantMap nested = value.toMap();
    wipeAssociative(nested);
  } else if (value.metaType() == QMetaType::fromType<QVariantHash>()) {
    QVariantHash nested = value.toHash();
    wipeAssociative(nested);
  } else if (value.metaType() == QMetaType::fromType<QStringList>()) {
    QStringList nested = value.toStringList();
    for (QString &entry : nested) {
      wipeString(entry);
    }
    nested.clear();
  } else if (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
    QVariant nested = value.value<QDBusVariant>().variant();
    wipeVariant(nested);
  }
  value.clear();
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

QByteArray takeSecretUtf8(QVariant &value) noexcept {
  if (value.metaType() != QMetaType::fromType<QString>()) {
    wipeVariant(value);
    return {};
  }
  auto *text = static_cast<QString *>(value.data());
  QByteArray result = text->toUtf8();
  wipeString(*text);
  value.clear();
  return result;
}

void wipeSettingsMap(NmSettingsMap &settings) noexcept {
  for (auto section = settings.begin(); section != settings.end(); ++section) {
    QString key = section.key();
    wipeString(key);
    wipeAssociative(section.value());
  }
  settings.clear();
}

void Private::wipeVariantValue(QVariant &value) noexcept { wipeVariant(value); }

void Private::wipeStringValue(QString &text) noexcept { wipeString(text); }

void Private::wipeByteArrayValue(QByteArray &bytes) noexcept {
  wipeByteArray(bytes);
}

} // namespace QindaQt::Network::SecretAgent
