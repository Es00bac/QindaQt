// SPDX-License-Identifier: GPL-3.0-or-later

#include "secret_agent_types_p.h"
#include "secret_request_admission_p.h"

#include <QtCore/QScopeGuard>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusVariant>

#include <algorithm>
#include <limits>

namespace QindaQt::Network::SecretAgent::Private {
namespace {

constexpr qsizetype kMaximumSections = 16;
constexpr qsizetype kMaximumProperties = 32;
constexpr qsizetype kMaximumTextBytes = 512;
constexpr qsizetype kMaximumAggregateBytes = 65'536;
constexpr qsizetype kMaximumNestedItems = 256;
constexpr int kMaximumVariantDepth = 8;

bool consumeBytes(const qsizetype bytes, qsizetype &aggregate) {
  if (bytes < 0 || bytes > kMaximumAggregateBytes - aggregate) {
    return false;
  }
  aggregate += bytes;
  return true;
}

// Counts one more element of a single container against the per-container
// item limit and charges its fixed overhead to the aggregate budget.
bool consumeElement(qsizetype &count, const qsizetype overhead,
                    qsizetype &aggregate) {
  return ++count <= kMaximumNestedItems && consumeBytes(overhead, aggregate);
}

bool consumeVariant(const QVariant &value, qsizetype &aggregate, int depth);

// ---------------------------------------------------------------------------
// Qt D-Bus wire forms.
//
// AGENT-NOTE: Inside a variant, Qt D-Bus unwraps only as and ay. Every other
// array or dictionary NetworkManager sends, including a nested a{sv} and empty
// arrays, stays a QDBusArgument. NetworkManager 1.56 sends ipv4/ipv6
// address-data and route-data (aa{sv}), legacy ipv4 addresses/routes (aau),
// legacy ipv6 addresses (a(ayuay)) and routes (a(ayuayu)) in every profile,
// ipv4.dns (au) and ipv6.dns (aay) when DNS servers are configured, and
// 802-3-ethernet.s390-options (a{ss}) in every wired profile. Rejecting these
// shapes refused ordinary secured Wi-Fi and wired 802.1X requests before the
// prompt. Each walker below is entered only after the dispatcher matched the
// exact signature, so the element reads cannot mismatch the wire type.
// ---------------------------------------------------------------------------

// au: one charged quint32 per element.
bool walkUIntArray(const QDBusArgument &cursor, qsizetype &aggregate,
                   int /*depth*/) {
  cursor.beginArray();
  qsizetype count = 0;
  while (!cursor.atEnd()) {
    if (!consumeElement(count, sizeof(quint32), aggregate)) {
      return false;
    }
    quint32 value = 0;
    cursor >> value;
  }
  cursor.endArray();
  return true;
}

// aau: legacy IPv4 address and route rows.
bool walkUIntArrays(const QDBusArgument &cursor, qsizetype &aggregate,
                    const int depth) {
  cursor.beginArray();
  qsizetype rows = 0;
  while (!cursor.atEnd()) {
    if (!consumeElement(rows, sizeof(QVariant), aggregate) ||
        !walkUIntArray(cursor, aggregate, depth + 1)) {
      return false;
    }
  }
  cursor.endArray();
  return true;
}

// aay: IPv6 DNS server addresses.
bool walkByteArrays(const QDBusArgument &cursor, qsizetype &aggregate,
                    int /*depth*/) {
  cursor.beginArray();
  qsizetype count = 0;
  while (!cursor.atEnd()) {
    if (!consumeElement(count, sizeof(QVariant), aggregate)) {
      return false;
    }
    QByteArray bytes;
    cursor >> bytes;
    const auto wipeBytes =
        qScopeGuard([&bytes] { wipeByteArrayValue(bytes); });
    if (!consumeBytes(bytes.size(), aggregate)) {
      return false;
    }
  }
  cursor.endArray();
  return true;
}

// a(ayuay) legacy IPv6 addresses; a(ayuayu) legacy IPv6 routes.
bool walkIpv6Records(const QDBusArgument &cursor, qsizetype &aggregate,
                     const bool hasMetric) {
  cursor.beginArray();
  qsizetype rows = 0;
  while (!cursor.atEnd()) {
    if (!consumeElement(rows, sizeof(QVariant), aggregate)) {
      return false;
    }
    cursor.beginStructure();
    QByteArray address;
    cursor >> address;
    const auto wipeAddress =
        qScopeGuard([&address] { wipeByteArrayValue(address); });
    quint32 prefix = 0;
    cursor >> prefix;
    QByteArray nextHop;
    cursor >> nextHop;
    const auto wipeNextHop =
        qScopeGuard([&nextHop] { wipeByteArrayValue(nextHop); });
    quint32 metric = 0;
    if (hasMetric) {
      cursor >> metric;
    }
    cursor.endStructure();
    if (!consumeBytes(address.size(), aggregate) ||
        !consumeBytes(sizeof(prefix), aggregate) ||
        !consumeBytes(nextHop.size(), aggregate) ||
        (hasMetric && !consumeBytes(sizeof(metric), aggregate))) {
      return false;
    }
  }
  cursor.endArray();
  return true;
}

bool walkIpv6Addresses(const QDBusArgument &cursor, qsizetype &aggregate,
                       int /*depth*/) {
  return walkIpv6Records(cursor, aggregate, false);
}

bool walkIpv6Routes(const QDBusArgument &cursor, qsizetype &aggregate,
                    int /*depth*/) {
  return walkIpv6Records(cursor, aggregate, true);
}

// a{ss}: wired s390-options, bond options, user data. Values are scrubbed
// because a{ss} is also NetworkManager's VPN secrets shape.
bool walkStringMap(const QDBusArgument &cursor, qsizetype &aggregate,
                   int /*depth*/) {
  cursor.beginMap();
  qsizetype entries = 0;
  while (!cursor.atEnd()) {
    if (!consumeElement(entries, sizeof(QVariant), aggregate)) {
      return false;
    }
    QString key;
    QString text;
    cursor.beginMapEntry();
    cursor >> key;
    const auto wipeKey = qScopeGuard([&key] { wipeStringValue(key); });
    cursor >> text;
    const auto wipeText = qScopeGuard([&text] { wipeStringValue(text); });
    cursor.endMapEntry();
    const bool accepted = boundedText(key) &&
                          consumeBytes(utf8ByteCount(key), aggregate) &&
                          boundedText(text, true) &&
                          consumeBytes(utf8ByteCount(text), aggregate);
    if (!accepted) {
      return false;
    }
  }
  cursor.endMap();
  return true;
}

// a{sv}: a vardict nested inside a variant. Its values sit one level below
// the map and are admitted by the ordinary variant walker.
bool walkVariantMap(const QDBusArgument &cursor, qsizetype &aggregate,
                    const int depth) {
  cursor.beginMap();
  qsizetype entries = 0;
  while (!cursor.atEnd()) {
    if (!consumeElement(entries, sizeof(QVariant), aggregate)) {
      return false;
    }
    QString key;
    QDBusVariant wrapped;
    cursor.beginMapEntry();
    cursor >> key;
    const auto wipeKey = qScopeGuard([&key] { wipeStringValue(key); });
    cursor >> wrapped;
    cursor.endMapEntry();
    QVariant entry = wrapped.variant();
    const auto wipeEntry =
        qScopeGuard([&entry] { wipeVariantValue(entry); });
    const bool accepted = boundedText(key) &&
                          consumeBytes(utf8ByteCount(key), aggregate) &&
                          consumeVariant(entry, aggregate, depth + 1);
    if (!accepted) {
      return false;
    }
  }
  cursor.endMap();
  return true;
}

// aa{sv}: address-data, route-data, routing-rules.
bool walkArrayOfMaps(const QDBusArgument &cursor, qsizetype &aggregate,
                     const int depth) {
  cursor.beginArray();
  qsizetype maps = 0;
  while (!cursor.atEnd()) {
    if (!consumeElement(maps, sizeof(QVariant), aggregate) ||
        !walkVariantMap(cursor, aggregate, depth + 1)) {
      return false;
    }
  }
  cursor.endArray();
  return true;
}

struct WireForm final {
  const char *signature;
  // Nesting levels below the property occupied by the deepest leaf.
  int leafDepth;
  bool (*walk)(const QDBusArgument &, qsizetype &, int);
};

constexpr WireForm kWireForms[] = {
    {"aa{sv}", 2, walkArrayOfMaps},     {"aau", 2, walkUIntArrays},
    {"a(ayuay)", 2, walkIpv6Addresses}, {"a(ayuayu)", 2, walkIpv6Routes},
    {"au", 1, walkUIntArray},           {"aay", 1, walkByteArrays},
    {"a{ss}", 1, walkStringMap},        {"a{sv}", 1, walkVariantMap},
};

bool consumeWire(const QDBusArgument wire, qsizetype &aggregate,
                 const int depth) {
  // currentSignature() is empty for a marshalling-direction argument, so an
  // in-process QDBusArgument never matches and fails closed.
  const QString signature = wire.currentSignature();
  const auto form =
      std::find_if(std::cbegin(kWireForms), std::cend(kWireForms),
                   [&signature](const WireForm &candidate) {
                     return signature == QLatin1StringView(candidate.signature);
                   });
  if (form == std::cend(kWireForms) ||
      depth + form->leafDepth > kMaximumVariantDepth) {
    return false;
  }
  // AGENT-GUARD: `wire` is a value copy of the argument stored in the inbound
  // map's QVariant. QDBusArgument is implicitly shared and its const read
  // operators detach a shared demarshaller before advancing, so reading here
  // never moves the stored argument. Do not read through QVariant::data() or
  // any other reference to the stored argument: that would consume the
  // caller's value in place.
  return form->walk(wire, aggregate, depth);
}

// ---------------------------------------------------------------------------
// Unwrapped Qt containers.
// ---------------------------------------------------------------------------

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

template <typename Associative>
bool consumeAssociative(const Associative &values, qsizetype &aggregate,
                        const int depth) {
  if (values.size() > kMaximumNestedItems ||
      !consumeBytes(values.size() * qsizetype(sizeof(QVariant)), aggregate)) {
    return false;
  }
  for (auto entry = values.keyValueBegin();
       entry != values.keyValueEnd(); ++entry) {
    const QString &key = entry->first;
    const QVariant &value = entry->second;
    if (!boundedText(key) ||
        !consumeBytes(utf8ByteCount(key), aggregate) ||
        !consumeVariant(value, aggregate, depth + 1)) {
      return false;
    }
  }
  return true;
}

bool consumeStrings(const QStringList &values, qsizetype &aggregate,
                    const int depth) {
  if (depth >= kMaximumVariantDepth || values.size() > kMaximumNestedItems ||
      !consumeBytes(values.size() * qsizetype(sizeof(QString)), aggregate)) {
    return false;
  }
  for (const QString &text : values) {
    if (!boundedText(text, true) ||
        !consumeBytes(utf8ByteCount(text), aggregate)) {
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
    const auto &text = *static_cast<const QString *>(value.constData());
    return boundedText(text, true) &&
           consumeBytes(utf8ByteCount(text), aggregate);
  }
  case QMetaType::QByteArray:
    return consumeBytes(
        static_cast<const QByteArray *>(value.constData())->size(), aggregate);
  case QMetaType::QStringList:
    return consumeStrings(
        *static_cast<const QStringList *>(value.constData()), aggregate, depth);
  case QMetaType::QVariantList:
    return consumeList(
        *static_cast<const QVariantList *>(value.constData()), aggregate,
        depth);
  case QMetaType::QVariantMap:
    return consumeAssociative(
        *static_cast<const QVariantMap *>(value.constData()), aggregate,
        depth);
  case QMetaType::QVariantHash:
    return consumeAssociative(
        *static_cast<const QVariantHash *>(value.constData()), aggregate,
        depth);
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
    if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
      return consumeWire(
          *static_cast<const QDBusArgument *>(value.constData()), aggregate,
          depth);
    }
    if (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
      const auto &wrapped =
          *static_cast<const QDBusVariant *>(value.constData());
      const QVariant nested = wrapped.variant();
      return consumeVariant(nested, aggregate, depth + 1);
    }
    return false;
  }
}

} // namespace

bool boundedText(const QString &text, const bool allowEmpty) {
  return (allowEmpty || !text.isEmpty()) && !text.contains(QChar::Null) &&
         utf8ByteCount(text) <= kMaximumTextBytes;
}

qsizetype utf8ByteCount(const QString &text) noexcept {
  // AGENT-GUARD: Admission must count secret text without materializing an
  // unwiped UTF-8 QByteArray. Count valid pairs as four bytes and lone
  // surrogates as the three-byte U+FFFD replacement so malformed UTF-16 cannot
  // bypass the text budget.
  qsizetype bytes = 0;
  constexpr qsizetype maximum = std::numeric_limits<qsizetype>::max();
  for (qsizetype index = 0; index < text.size(); ++index) {
    const quint16 first = text.at(index).unicode();
    qsizetype encoded = 0;
    if (first <= 0x7fU) {
      encoded = 1;
    } else if (first <= 0x7ffU) {
      encoded = 2;
    } else if (first >= 0xd800U && first <= 0xdbffU &&
               index + 1 < text.size()) {
      const quint16 second = text.at(index + 1).unicode();
      if (second >= 0xdc00U && second <= 0xdfffU) {
        encoded = 4;
        ++index;
      } else {
        encoded = 3;
      }
    } else {
      encoded = 3;
    }
    if (bytes > maximum - encoded) {
      return maximum;
    }
    bytes += encoded;
  }
  return bytes;
}

bool boundedConnection(const NmSettingsMap &connection) {
  if (connection.isEmpty() || connection.size() > kMaximumSections) {
    return false;
  }
  qsizetype aggregate = 0;
  for (auto section = connection.keyValueBegin();
       section != connection.keyValueEnd(); ++section) {
    const QString &sectionName = section->first;
    const QVariantMap &properties = section->second;
    if (!boundedText(sectionName) ||
        properties.size() > kMaximumProperties) {
      return false;
    }
    if (!consumeBytes(utf8ByteCount(sectionName), aggregate)) {
      return false;
    }
    for (auto property = properties.keyValueBegin();
         property != properties.keyValueEnd(); ++property) {
      const QString &propertyName = property->first;
      const QVariant &propertyValue = property->second;
      if (!boundedText(propertyName)) {
        return false;
      }
      if (!consumeBytes(utf8ByteCount(propertyName), aggregate) ||
          !consumeVariant(propertyValue, aggregate, 0)) {
        return false;
      }
    }
  }
  return true;
}

} // namespace QindaQt::Network::SecretAgent::Private
