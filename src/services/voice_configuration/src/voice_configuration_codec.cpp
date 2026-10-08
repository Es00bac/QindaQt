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
    const QStringList codes{u"ready"_qs, u"reload_required"_qs, u"busy"_qs,
        u"ok"_qs, u"uncertain"_qs, u"timeout"_qs, u"invalid_key"_qs,
        u"malformed_request"_qs, u"revision_stale"_qs, u"keyring_unavailable"_qs,
        u"credential_unavailable"_qs, u"saved_reload_unconfirmed"_qs, u"reload_failed"_qs};
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
    quint64 version = 0; return number(map.value(u"schemaVersion"_qs), version) && version == 1;
}
}
bool validKey(const QString &key) {
    if (key.isEmpty() || key.size() > MaximumKeyBytes) return false;
    for (const QChar c : key) if (c.unicode() < 33 || c.unicode() > 126) return false;
    return true;
}
bool decodeSnapshot(const QVariantMap &map, Snapshot &destination) {
    if (!keys(map, {u"schemaVersion"_qs, u"revision"_qs, u"configuredProvider"_qs,
        u"effectiveProvider"_qs, u"credentialSource"_qs, u"statusCode"_qs,
        u"fallbackActive"_qs, u"credentialCached"_qs, u"environmentOverride"_qs,
        u"canConfigure"_qs}) || !schema(map)) return false;
    Snapshot value;
    if (!number(map.value(u"revision"_qs), value.revision) || value.revision == 0
        || !identifier(map.value(u"configuredProvider"_qs), value.configuredProvider)
        || !identifier(map.value(u"effectiveProvider"_qs), value.effectiveProvider)
        || !identifier(map.value(u"credentialSource"_qs), value.credentialSource)
        || !code(map.value(u"statusCode"_qs), value.statusCode)
        || !flag(map.value(u"fallbackActive"_qs), value.fallbackActive)
        || !flag(map.value(u"credentialCached"_qs), value.credentialCached)
        || !flag(map.value(u"environmentOverride"_qs), value.environmentOverride)
        || !flag(map.value(u"canConfigure"_qs), value.canConfigure)) return false;
    if (!QStringList{u"none"_qs,u"secret-service"_qs,u"environment"_qs,u"unresolved"_qs}
            .contains(value.credentialSource)) return false;
    if (value.fallbackActive != (value.configuredProvider != value.effectiveProvider)
        || value.environmentOverride != (value.credentialSource == u"environment"_qs)) return false;
    destination = value; return true;
}
bool decodeResult(const QVariantMap &map, Result &destination) {
    if (!keys(map, {u"schemaVersion"_qs,u"requestId"_qs,u"revision"_qs,
        u"operation"_qs,u"status"_qs,u"reasonCode"_qs}) || !schema(map)) return false;
    Result value; quint64 operation = 0, status = 0;
    if (!number(map.value(u"requestId"_qs), value.requestId) || value.requestId == 0
        || !number(map.value(u"revision"_qs), value.revision) || value.revision == 0
        || !number(map.value(u"operation"_qs), operation) || operation > 1
        || !number(map.value(u"status"_qs), status) || status > 5
        || !code(map.value(u"reasonCode"_qs), value.reasonCode)) return false;
    value.operation = static_cast<Operation>(operation);
    value.status = static_cast<Status>(status); destination = value; return true;
}
}
