// SPDX-License-Identifier: GPL-3.0-or-later

#include "secret_request_policy_p.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QRegularExpression>
#include <QtCore/QSet>

#include <algorithm>

namespace QindaQt::Network::SecretAgent::Private {
namespace {

constexpr qsizetype kMaximumSections = 16;
constexpr qsizetype kMaximumProperties = 32;
constexpr qsizetype kMaximumHints = 16;
constexpr qsizetype kMaximumTextBytes = 512;
constexpr qsizetype kMaximumAggregateBytes = 65'536;
constexpr qsizetype kMaximumNestedItems = 256;
constexpr int kMaximumVariantDepth = 8;

QString tr(const char *text) {
  return QCoreApplication::translate("NetworkSecretPrompt", text);
}

bool boundedText(const QString &text, const bool allowEmpty = false) {
  return (allowEmpty || !text.isEmpty()) && !text.contains(QChar::Null) &&
         text.toUtf8().size() <= kMaximumTextBytes;
}

bool consumeBytes(const qsizetype bytes, qsizetype &aggregate) {
  if (bytes < 0 || bytes > kMaximumAggregateBytes - aggregate) {
    return false;
  }
  aggregate += bytes;
  return true;
}

bool consumeVariant(const QVariant &value, qsizetype &aggregate,
                    const int depth);

bool consumeList(const QVariantList &values, qsizetype &aggregate,
                 const int depth) {
  if (values.size() > kMaximumNestedItems ||
      !consumeBytes(values.size() * qsizetype(sizeof(QVariant)), aggregate)) {
    return false;
  }
  return std::all_of(values.cbegin(), values.cend(),
                     [&aggregate, depth](const QVariant &entry) {
                       return consumeVariant(entry, aggregate, depth + 1);
                     });
}

bool consumeMap(const QVariantMap &values, qsizetype &aggregate,
                const int depth) {
  if (values.size() > kMaximumNestedItems ||
      !consumeBytes(values.size() * qsizetype(sizeof(QVariant)), aggregate)) {
    return false;
  }
  for (auto entry = values.cbegin(); entry != values.cend(); ++entry) {
    if (!boundedText(entry.key()) ||
        !consumeBytes(entry.key().toUtf8().size(), aggregate) ||
        !consumeVariant(entry.value(), aggregate, depth + 1)) {
      return false;
    }
  }
  return true;
}

bool consumeHash(const QVariantHash &values, qsizetype &aggregate,
                 const int depth) {
  if (values.size() > kMaximumNestedItems ||
      !consumeBytes(values.size() * qsizetype(sizeof(QVariant)), aggregate)) {
    return false;
  }
  for (auto entry = values.cbegin(); entry != values.cend(); ++entry) {
    if (!boundedText(entry.key()) ||
        !consumeBytes(entry.key().toUtf8().size(), aggregate) ||
        !consumeVariant(entry.value(), aggregate, depth + 1)) {
      return false;
    }
  }
  return true;
}

bool consumeStrings(const QStringList &values, qsizetype &aggregate,
                    const int depth) {
  if (depth >= kMaximumVariantDepth ||
      values.size() > kMaximumNestedItems ||
      !consumeBytes(values.size() * qsizetype(sizeof(QString)), aggregate)) {
    return false;
  }
  for (const QString &text : values) {
    if (!boundedText(text, true) ||
        !consumeBytes(text.toUtf8().size(), aggregate)) {
      return false;
    }
  }
  return true;
}

bool consumeVariant(const QVariant &value, qsizetype &aggregate,
                    const int depth) {
  if (!value.isValid() || depth > kMaximumVariantDepth) {
    return false;
  }
  switch (value.typeId()) {
  case QMetaType::QString: {
    const QString text = value.toString();
    return boundedText(text, true) &&
           consumeBytes(text.toUtf8().size(), aggregate);
  }
  case QMetaType::QByteArray:
    return consumeBytes(value.toByteArray().size(), aggregate);
  case QMetaType::QStringList:
    return consumeStrings(value.toStringList(), aggregate, depth);
  case QMetaType::QVariantList:
    return consumeList(value.toList(), aggregate, depth);
  case QMetaType::QVariantMap:
    return consumeMap(value.toMap(), aggregate, depth);
  case QMetaType::QVariantHash:
    return consumeHash(value.toHash(), aggregate, depth);
  case QMetaType::Bool:
    return consumeBytes(sizeof(bool), aggregate);
  case QMetaType::Char:
  case QMetaType::SChar:
  case QMetaType::UChar:
    return consumeBytes(sizeof(char), aggregate);
  case QMetaType::Short:
  case QMetaType::UShort:
    return consumeBytes(sizeof(short), aggregate);
  case QMetaType::Int:
  case QMetaType::UInt:
  case QMetaType::Float:
    return consumeBytes(sizeof(quint32), aggregate);
  case QMetaType::LongLong:
  case QMetaType::ULongLong:
  case QMetaType::Double:
    return consumeBytes(sizeof(quint64), aggregate);
  default:
    return false;
  }
}

bool boundedConnection(const NmSettingsMap &connection) {
  if (connection.isEmpty() || connection.size() > kMaximumSections) {
    return false;
  }
  qsizetype aggregate = 0;
  for (auto section = connection.cbegin(); section != connection.cend();
       ++section) {
    if (!boundedText(section.key()) ||
        section.value().size() > kMaximumProperties) {
      return false;
    }
    if (!consumeBytes(section.key().toUtf8().size(), aggregate)) {
      return false;
    }
    for (auto property = section.value().cbegin();
         property != section.value().cend(); ++property) {
      if (!boundedText(property.key())) {
        return false;
      }
      if (!consumeBytes(property.key().toUtf8().size(), aggregate) ||
          !consumeVariant(property.value(), aggregate, 0)) {
        return false;
      }
    }
  }
  return true;
}

QString connectionName(const NmSettingsMap &connection) {
  const auto section = connection.constFind(QStringLiteral("connection"));
  if (section == connection.cend()) {
    return {};
  }
  const QString id = section->value(QStringLiteral("id")).toString();
  const QString uuid = section->value(QStringLiteral("uuid")).toString();
  static const QRegularExpression uuidPattern(
      QStringLiteral("^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[1-5][0-9A-Fa-f]{3}-"
                     "[89ABab][0-9A-Fa-f]{3}-[0-9A-Fa-f]{12}$"));
  if (!boundedText(id) || !uuidPattern.match(uuid).hasMatch()) {
    return {};
  }
  // The inbound DBus map is securely cleared after admission. Keep only an
  // independent non-secret display allocation in the pending prompt.
  return QString(id.constData(), id.size());
}

bool validHints(const QStringList &hints) {
  if (hints.size() > kMaximumHints) {
    return false;
  }
  return std::all_of(hints.cbegin(), hints.cend(),
                     [](const QString &hint) { return boundedText(hint); });
}

PromptField field(const QString &key) {
  if (key == QStringLiteral("psk")) {
    return {key, tr("Wi-Fi password"), true, 64};
  }
  if (key.startsWith(QStringLiteral("wep-key"))) {
    return {key, tr("WEP key"), true, 64};
  }
  if (key == QStringLiteral("identity")) {
    return {key, tr("Identity"), false, 253};
  }
  return {key, tr("Password"), true, 256};
}

QStringList wifiFields(const QVariantMap &setting, const QStringList &hints) {
  if (!hints.isEmpty()) {
    QStringList result;
    for (const QString &hint : hints) {
      if (hint == QStringLiteral("psk") ||
          QRegularExpression(QStringLiteral("^wep-key[0-3]$"))
              .match(hint)
              .hasMatch()) {
        if (!result.contains(hint)) {
          result.append(hint);
        }
      } else {
        return {};
      }
    }
    return result;
  }
  const QString keyManagement =
      setting.value(QStringLiteral("key-mgmt")).toString();
  if (keyManagement == QStringLiteral("none")) {
    const quint32 index =
        std::min(setting.value(QStringLiteral("wep-tx-keyidx")).toUInt(), 3U);
    return {QStringLiteral("wep-key%1").arg(index)};
  }
  if (keyManagement == QStringLiteral("wpa-psk") ||
      keyManagement == QStringLiteral("sae")) {
    return {QStringLiteral("psk")};
  }
  return {};
}

QStringList enterpriseFields(const QStringList &hints) {
  if (hints.isEmpty()) {
    return {QStringLiteral("identity"), QStringLiteral("password")};
  }
  QStringList result;
  for (const QString &hint : hints) {
    if (hint != QStringLiteral("identity") &&
        hint != QStringLiteral("password")) {
      return {};
    }
    if (!result.contains(hint)) {
      result.append(hint);
    }
  }
  return result;
}

} // namespace

QString flagsKey(const QString &key) {
  if (key == QStringLiteral("psk")) {
    return QStringLiteral("psk-flags");
  }
  if (key.startsWith(QStringLiteral("wep-key"))) {
    return QStringLiteral("wep-key-flags");
  }
  if (key == QStringLiteral("password")) {
    return QStringLiteral("password-flags");
  }
  return {};
}

std::optional<PromptRequest> promptFor(const GetSecretsRequest &request,
                                       const quint64 requestId) {
  if (requestId == 0 || request.caller.isEmpty() ||
      request.caller != request.networkManagerOwner ||
      (request.flags & ~kKnownGetSecretsFlags) != 0U ||
      (request.flags & kAllowInteraction) == 0U ||
      !boundedConnection(request.connection) || !validHints(request.hints)) {
    return std::nullopt;
  }
  const QString name = connectionName(request.connection);
  if (name.isEmpty()) {
    return std::nullopt;
  }
  const auto setting = request.connection.constFind(request.settingName);
  if (setting == request.connection.cend()) {
    return std::nullopt;
  }
  QStringList keys;
  if (request.settingName == QStringLiteral("802-11-wireless-security")) {
    keys = wifiFields(*setting, request.hints);
  } else if (request.settingName == QStringLiteral("802-1x")) {
    keys = enterpriseFields(request.hints);
  }
  if (keys.isEmpty()) {
    return std::nullopt;
  }
  PromptRequest result{
      requestId, name, request.connectionPath, request.settingName, {}};
  result.fields.reserve(keys.size());
  for (const QString &key : std::as_const(keys)) {
    result.fields.append(field(key));
  }
  return result;
}

SecretReply replyFor(const PromptRequest &request, PromptResult &result) {
  SecretReply reply;
  reply.settingName = request.settingName;
  reply.remember = result.remember;
  const qsizetype submittedCount = result.values.size();
  QSet<QString> seen;
  for (const PromptField &field : request.fields) {
    const auto valueIt =
        std::find_if(result.values.begin(), result.values.end(),
                     [&field](const SecretValue &candidate) {
                       return candidate.key == field.key;
                     });
    if (valueIt == result.values.end() || seen.contains(valueIt->key) ||
        valueIt->bytes.isEmpty() || valueIt->bytes.contains('\0') ||
        valueIt->bytes.size() > field.maximumLength ||
        QString::fromUtf8(valueIt->bytes).toUtf8() != valueIt->bytes) {
      reply.wipe();
      result.wipe();
      return {};
    }
    seen.insert(valueIt->key);
    reply.values.append({field.key, std::move(valueIt->bytes)});
  }
  if (reply.values.size() != request.fields.size() ||
      seen.size() != submittedCount) {
    reply.wipe();
    result.wipe();
    return {};
  }
  result.wipe();
  return reply;
}

} // namespace QindaQt::Network::SecretAgent::Private
