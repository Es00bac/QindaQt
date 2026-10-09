// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/android_runtime/protocol.h"
#include <QStringDecoder>
#include <QUuid>

namespace QindaQt::Services::AndroidRuntime {
using namespace ShellLauncher;
namespace {
bool uuid(const QString &value) {
    const QUuid id = QUuid::fromString(value);
    return value.size() == 36 && !id.isNull() && id.toString(QUuid::WithoutBraces) == value;
}
struct Writer {
    QByteArray bytes;
    bool ok = true;
    void raw(const QByteArray &value) {
        if (!ok || value.size() > maxPacketBytes - bytes.size()) { ok = false; return; }
        bytes += value;
    }
    void number(quint64 value, int width) {
        for (int i=0; i<width; ++i) { raw(QByteArray(1, static_cast<char>(value & 255))); value >>= 8; }
    }
    void text(const QString &value, qsizetype limit) {
        if (value.size() > limit) { ok = false; return; }
        const auto utf8=value.toUtf8();
        if (utf8.size() > limit) { ok=false; return; }
        number(static_cast<quint64>(utf8.size()),2); raw(utf8);
    }
    void binding(const ManagedApplicationBinding &value) {
        number(static_cast<quint8>(value.origin),1); text(value.registrationId,36);
        number(value.registrationRevision,8); text(value.recipeDigest,64);
    }
    void list(const QStringList &values) {
        if (values.size()>16) { ok=false; return; }
        number(static_cast<quint64>(values.size()),1);
        for (const auto &value:values) text(value,64);
    }
};
struct Reader {
    const QByteArray &bytes;
    qsizetype offset=0;
    bool ok=true;
    QByteArray raw(qsizetype size) {
        if (!ok || size<0 || size>bytes.size()-offset) { ok=false; return {}; }
        const auto result=bytes.mid(offset,size); offset+=size; return result;
    }
    quint64 number(int width) {
        const auto value=raw(width); quint64 result=0;
        for (qsizetype i=0;i<value.size();++i)
            result |= quint64(static_cast<quint8>(value.at(i))) << (8*static_cast<unsigned>(i));
        return result;
    }
    QString text(qsizetype limit) {
        const auto size=number(2);
        if (!ok || size>static_cast<quint64>(limit) || size>static_cast<quint64>(bytes.size()-offset)) { ok=false; return {}; }
        const auto value=raw(static_cast<qsizetype>(size));
        QStringDecoder utf8(QStringDecoder::Utf8); const QString decoded=utf8(value);
        if (utf8.hasError()) ok=false;
        return decoded;
    }
    ManagedApplicationBinding binding() {
        ManagedApplicationBinding value;
        value.origin=static_cast<ApplicationOrigin>(number(1)); value.registrationId=text(36);
        value.registrationRevision=number(8); value.recipeDigest=text(64);
        if (!isValidManagedApplicationBinding(value)) ok=false;
        return value;
    }
    QStringList list() {
        const auto count=number(1); QStringList values;
        if (count>16) { ok=false; return {}; }
        for (quint64 i=0;i<count && ok;++i) values.append(text(64));
        return values;
    }
};
bool validRequest(const LaunchRequest &request) {
    return uuid(request.operationId) && uuid(request.attachmentId) && request.snapshotRevision>0
        && isValidManagedApplicationBinding(request.application);
}
bool validResult(const LaunchResult &result) {
    return uuid(result.operationId) && uuid(result.attachmentId)
        && isValidManagedApplicationBinding(result.application) && validRuntimeMessage(result.message)
        && result.phase>=LaunchPhase::Accepted && result.phase<=LaunchPhase::OutcomeUnknown;
}
}
QByteArray encodePacket(const Packet &packet)
{
    if (packet.channelKey.size()!=32) return {};
    Writer out;
    out.raw(QByteArrayLiteral("QAR1")); out.number(static_cast<quint8>(packet.kind),1); out.raw(packet.channelKey);
    switch (packet.kind) {
    case PacketKind::Snapshot: {
        const auto *snapshot=std::get_if<RuntimeSnapshot>(&packet.value);
        if (!snapshot || !validateRuntimeSnapshot(*snapshot)) return {};
        out.text(snapshot->attachmentId,36); out.number(snapshot->revision,8);
        out.number(static_cast<quint8>(snapshot->state),1); out.text(snapshot->message,1024);
        out.number(static_cast<quint64>(snapshot->registrations.size()),2);
        for (const auto &registration:snapshot->registrations) {
            out.binding(registration.binding); out.text(registration.name,256);
            out.text(registration.genericName,256); out.text(registration.comment,512);
            out.text(registration.iconName,128); out.list(registration.categories); out.list(registration.keywords);
            const ApplicationStatus *status=nullptr;
            for (const auto &candidate:snapshot->applications)
                if (candidate.binding==registration.binding) { status=&candidate; break; }
            if (!status) return {};
            out.number(static_cast<quint8>(status->state),1); out.number(status->windowCount.has_value()?1:0,1);
            if (status->windowCount) out.number(*status->windowCount,4);
            out.number(status->launchAvailable?1:0,1); out.text(status->message,1024);
            if (!out.ok) return {};
        }
        break;
    }
    case PacketKind::Refresh:
        if (!std::holds_alternative<std::monostate>(packet.value)) return {};
        break;
    case PacketKind::Launch: {
        const auto *request=std::get_if<LaunchRequest>(&packet.value);
        if (!request || !validRequest(*request)) return {};
        out.text(request->operationId,36); out.text(request->attachmentId,36);
        out.number(request->snapshotRevision,8); out.binding(request->application); break;
    }
    case PacketKind::Result: {
        const auto *result=std::get_if<LaunchResult>(&packet.value);
        if (!result || !validResult(*result)) return {};
        out.text(result->operationId,36); out.text(result->attachmentId,36); out.binding(result->application);
        out.number(static_cast<quint8>(result->phase),1); out.text(result->message,1024); break;
    }
    default: return {};
    }
    return out.ok ? out.bytes : QByteArray();
}
std::optional<Packet> decodePacket(const QByteArray &bytes)
{
    if (bytes.size()>maxPacketBytes || bytes.size()<37) return std::nullopt;
    Reader in{bytes};
    if (in.raw(4)!=QByteArrayLiteral("QAR1")) return std::nullopt;
    Packet packet; packet.kind=static_cast<PacketKind>(in.number(1)); packet.channelKey=in.raw(32);
    switch (packet.kind) {
    case PacketKind::Snapshot: {
        RuntimeSnapshot snapshot;
        snapshot.attachmentId=in.text(36); snapshot.revision=in.number(8);
        snapshot.state=static_cast<RuntimeState>(in.number(1)); snapshot.message=in.text(1024);
        const auto count=in.number(2);
        // AGENT-GUARD: Even a bounded packet may declare a huge array. Refuse
        // impossible counts before reserving; a row needs at least128 bytes.
        if (count>4096 || count*128>static_cast<quint64>(bytes.size()-in.offset)) return std::nullopt;
        for (quint64 i=0;i<count && in.ok;++i) {
            ManagedApplicationRegistration registration;
            registration.binding=in.binding(); registration.name=in.text(256);
            registration.genericName=in.text(256); registration.comment=in.text(512);
            registration.iconName=in.text(128); registration.categories=in.list(); registration.keywords=in.list();
            ApplicationStatus status; status.binding=registration.binding;
            status.state=static_cast<ApplicationState>(in.number(1)); const auto known=in.number(1);
            if (known>1) in.ok=false;
            if (known==1) status.windowCount=static_cast<quint32>(in.number(4));
            const auto available=in.number(1); if (available>1) in.ok=false;
            status.launchAvailable=available==1; status.message=in.text(1024);
            snapshot.registrations.append(registration); snapshot.applications.append(status);
        }
        if (!in.ok || !validateRuntimeSnapshot(snapshot)) return std::nullopt;
        packet.value=snapshot; break;
    }
    case PacketKind::Refresh: break;
    case PacketKind::Launch: {
        LaunchRequest request;
        request.operationId=in.text(36); request.attachmentId=in.text(36);
        request.snapshotRevision=in.number(8); request.application=in.binding();
        if (!validRequest(request)) return std::nullopt;
        packet.value=request; break;
    }
    case PacketKind::Result: {
        LaunchResult result;
        result.operationId=in.text(36); result.attachmentId=in.text(36); result.application=in.binding();
        result.phase=static_cast<LaunchPhase>(in.number(1)); result.message=in.text(1024);
        if (!validResult(result)) return std::nullopt;
        packet.value=result; break;
    }
    default: return std::nullopt;
    }
    return in.ok && in.offset==bytes.size() ? std::optional(packet) : std::nullopt;
}
} // namespace QindaQt::Services::AndroidRuntime
