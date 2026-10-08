// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/voice_configuration/voice_configuration_codec.h>
#include <QtCore/QStringList>
#include <limits>
using namespace Qt::StringLiterals;
namespace QindaQt::Services::VoiceConfiguration {
namespace {
bool number(const QVariant &v, quint64 &out) {
    switch (v.metaType().id()) {
    case QMetaType::Int: case QMetaType::LongLong:
        if (v.toLongLong() < 0) return false;
        break;
    case QMetaType::UInt: case QMetaType::ULongLong: break;
    default: return false;
    }
    out = v.toULongLong(); return true;
}
bool identifier(const QVariant &v, QString &out) {
    if (v.metaType().id() != QMetaType::QString) return false;
    const QString text = v.toString();
    if (text.isEmpty() || text.size() > MaximumIdentifierBytes) return false;
    for (const QChar c : text)
        if (!((c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z')
              || (c >= u'0' && c <= u'9') || c == u'_' || c == u'-')) return false;
    out = text; return true;
}
bool code(const QVariant &v, QString &out) {
    if (!identifier(v, out)) return false;
    const QStringList codes{u"ready"_s, u"reload_required"_s, u"busy"_s,
        u"ok"_s, u"uncertain"_s, u"timeout"_s, u"invalid_key"_s,
        u"malformed_request"_s, u"revision_stale"_s, u"keyring_unavailable"_s,
        u"credential_unavailable"_s, u"saved_reload_unconfirmed"_s, u"reload_failed"_s};
    return codes.contains(out);
}
bool keys(const QVariantMap &map, const QStringList &expected) {
    if (map.size() != expected.size()) return false;
    for (const QString &key : expected) if (!map.contains(key)) return false;
    return true;
}
bool flag(const QVariant &v, bool &out) {
    if (v.metaType().id() != QMetaType::Bool) return false;
    out = v.toBool(); return true;
}
bool schema(const QVariantMap &map) {
    quint64 version = 0; return number(map.value(u"schemaVersion"_s), version) && version == 1;
}
}
bool validKey(const QString &key) {
    if (key.isEmpty() || key.size() > MaximumKeyBytes) return false;
    for (const QChar c : key) if (c.unicode() < 33 || c.unicode() > 126) return false;
    return true;
}
bool decodeSnapshot(const QVariantMap &map, Snapshot &destination) {
    if (!keys(map, {u"schemaVersion"_s, u"revision"_s, u"configuredProvider"_s,
        u"effectiveProvider"_s, u"credentialSource"_s, u"statusCode"_s,
        u"fallbackActive"_s, u"credentialCached"_s, u"environmentOverride"_s,
        u"canConfigure"_s}) || !schema(map)) return false;
    Snapshot value;
    if (!number(map.value(u"revision"_s), value.revision) || value.revision == 0
        || !identifier(map.value(u"configuredProvider"_s), value.configuredProvider)
        || !identifier(map.value(u"effectiveProvider"_s), value.effectiveProvider)
        || !identifier(map.value(u"credentialSource"_s), value.credentialSource)
        || !code(map.value(u"statusCode"_s), value.statusCode)
        || !flag(map.value(u"fallbackActive"_s), value.fallbackActive)
        || !flag(map.value(u"credentialCached"_s), value.credentialCached)
        || !flag(map.value(u"environmentOverride"_s), value.environmentOverride)
        || !flag(map.value(u"canConfigure"_s), value.canConfigure)) return false;
    if (!QStringList{u"none"_s,u"secret-service"_s,u"environment"_s,u"unresolved"_s}
            .contains(value.credentialSource)) return false;
    if (value.fallbackActive != (value.configuredProvider != value.effectiveProvider)
        || value.environmentOverride != (value.credentialSource == u"environment"_s)) return false;
    destination = value; return true;
}
bool decodeResult(const QVariantMap &map, Result &destination) {
    if (!keys(map, {u"schemaVersion"_s,u"requestId"_s,u"revision"_s,
        u"operation"_s,u"status"_s,u"reasonCode"_s}) || !schema(map)) return false;
    Result value; quint64 operation = 0, status = 0;
    if (!number(map.value(u"requestId"_s), value.requestId) || value.requestId == 0
        || !number(map.value(u"revision"_s), value.revision) || value.revision == 0
        || !number(map.value(u"operation"_s), operation) || operation > 1
        || !number(map.value(u"status"_s), status) || status > 5
        || !code(map.value(u"reasonCode"_s), value.reasonCode)) return false;
    value.operation = static_cast<Operation>(operation);
    value.status = static_cast<Status>(status); destination = value; return true;
}
}
