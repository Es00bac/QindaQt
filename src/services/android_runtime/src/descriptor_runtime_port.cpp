// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/android_runtime/descriptor_runtime_port.h"
#include "packet_channel_p.h"
#include <QHash>
#include <QSocketNotifier>
#include <QPointer>
#include <QSet>

namespace QindaQt::Services::AndroidRuntime {
namespace {
bool sameRequest(const LaunchRequest &a,const LaunchRequest &b) {
    return a.operationId==b.operationId && a.attachmentId==b.attachmentId
        && a.snapshotRevision==b.snapshotRevision && a.application==b.application;
}
bool admittedLaunch(const RuntimeSnapshot &snapshot,const LaunchRequest &request) {
    if (snapshot.state!=RuntimeState::Ready || snapshot.attachmentId!=request.attachmentId
        || snapshot.revision!=request.snapshotRevision) return false;
    for (const auto &status:snapshot.applications)
        if (status.binding==request.application)
            return status.launchAvailable && (status.state==ApplicationState::Ready || status.state==ApplicationState::Running);
    return false;
}
}
class DescriptorRuntimePort::Private {
public:
    struct Remembered { LaunchRequest request; LaunchSubmission submission; bool terminal=false; std::optional<LaunchResult> result=std::nullopt; };
    DescriptorRuntimePort &owner;
    PacketChannel channel;
    QByteArray key;
    RuntimeSnapshot current;
    QHash<QString,Remembered> ledger;
    QSet<QString> retiredAttachments;
    std::unique_ptr<QSocketNotifier> input;
    std::unique_ptr<QSocketNotifier> lifetime;
    Private(DescriptorRuntimePort &port,int socket,int pidfd,int executable,int pid,unsigned uid,QByteArray secret)
        : owner(port),channel(socket,pidfd,executable,pid,uid),key(std::move(secret)) {}
    void revoke(const QString &reason) {
        if (input) input->setEnabled(false);
        if (lifetime) lifetime->setEnabled(false);
        channel.close(); current={};
        // Mark everything before signals: consumers may reenter and mutate.
        QVector<LaunchResult> outcomes;
        for (auto it=ledger.begin();it!=ledger.end();++it) {
            if (it->submission.accepted && !it->terminal) {
                it->terminal=true;
                it->result=LaunchResult{it->request.operationId,it->request.attachmentId,
                    it->request.application,LaunchPhase::OutcomeUnknown,reason};
                outcomes.append(*it->result);
            }
        }
        QPointer<DescriptorRuntimePort> guard(&owner);
        for (const auto &outcome:outcomes) { if (!guard) return; Q_EMIT guard->launchFinished(outcome); }
        if (guard) Q_EMIT guard->snapshotChanged();
    }
    void publish(const RuntimeSnapshot &snapshot) {
        if (!snapshot.attachmentId.isEmpty() && retiredAttachments.contains(snapshot.attachmentId)) {
            revoke(QStringLiteral("Retired Android runtime attachment was reused")); return;
        }
        if (snapshot.attachmentId!=current.attachmentId && !current.attachmentId.isEmpty()) {
            if (retiredAttachments.size()>=64) { revoke(QStringLiteral("Android runtime attachment history is full")); return; }
            retiredAttachments.insert(current.attachmentId);
        }
        if (!current.attachmentId.isEmpty() && snapshot.attachmentId==current.attachmentId) {
            if (snapshot.revision<current.revision || (snapshot.revision==current.revision && snapshot!=current)) {
                revoke(QStringLiteral("Android producer published stale status")); return;
            }
            if (snapshot==current) return;
        }
        current=snapshot;
        QVector<LaunchResult> outcomes;
        for (auto it=ledger.begin();it!=ledger.end();++it) {
            if (!it->submission.accepted || it->terminal) continue;
            bool retained=current.attachmentId==it->request.attachmentId;
            if (retained) {
                retained=false;
                for (const auto &registration:current.registrations)
                    retained |= registration.binding==it->request.application;
            }
            if (!retained) {
                it->terminal=true;
                it->result=LaunchResult{it->request.operationId,it->request.attachmentId,
                    it->request.application,LaunchPhase::OutcomeUnknown,QStringLiteral("Android registration or runtime changed")};
                outcomes.append(*it->result);
            }
        }
        QPointer<DescriptorRuntimePort> guard(&owner);
        for (const auto &outcome:outcomes) { if (!guard) return; Q_EMIT guard->launchFinished(outcome); }
        if (guard) Q_EMIT guard->snapshotChanged();
    }
    void drain() {
        QPointer<DescriptorRuntimePort> guard(&owner);
        for (int i=0;i<64;++i) {
            const auto received=channel.read();
            if (received.state==ReadPacket::Empty) return;
            if (received.state!=ReadPacket::Received) { revoke(QStringLiteral("Android producer channel was lost or rejected")); return; }
            const auto packet=decodePacket(received.bytes);
            if (!packet || packet->channelKey!=key) { revoke(QStringLiteral("Android producer packet was rejected")); return; }
            if (packet->kind==PacketKind::Snapshot) { publish(std::get<RuntimeSnapshot>(packet->value)); }
            else if (packet->kind==PacketKind::Result) {
                const auto result=std::get<LaunchResult>(packet->value);
                const auto it=ledger.find(result.operationId);
                if (it==ledger.end() || result.attachmentId!=it->request.attachmentId
                    || result.application!=it->request.application) { revoke(QStringLiteral("Android launch result did not match its request")); return; }
                if (it->terminal) continue;
                it->terminal=result.phase!=LaunchPhase::Accepted;
                it->result=result;
                Q_EMIT owner.launchFinished(result);
            } else { revoke(QStringLiteral("Android producer packet direction was rejected")); return; }
            if (!guard) return;
            if (!channel.live()) { revoke(QStringLiteral("Android producer lifetime ended")); return; }
        }
    }
};
DescriptorRuntimePort::DescriptorRuntimePort(int socket,int pidfd,int executable,int pid,unsigned uid,QByteArray key,QObject *parent)
    : RuntimePort(parent),d(std::make_unique<Private>(*this,socket,pidfd,executable,pid,uid,std::move(key)))
{
    if (d->key.size()!=32 || !d->channel.live()) { d->channel.close(); return; }
    d->input=std::make_unique<QSocketNotifier>(d->channel.socketFd(),QSocketNotifier::Read,this);
    d->lifetime=std::make_unique<QSocketNotifier>(d->channel.lifetimeFd(),QSocketNotifier::Read,this);
    connect(d->input.get(),&QSocketNotifier::activated,this,[this]{ d->drain(); });
    connect(d->lifetime.get(),&QSocketNotifier::activated,this,[this]{ d->revoke(QStringLiteral("Android producer exited before its outcome was confirmed")); });
}
DescriptorRuntimePort::~DescriptorRuntimePort()=default;
bool DescriptorRuntimePort::producerLive() const { return d->channel.live(); }
RuntimeSnapshot DescriptorRuntimePort::snapshot() const { return producerLive()?d->current:RuntimeSnapshot{}; }
void DescriptorRuntimePort::requestRefresh()
{
    if (!producerLive()) { d->revoke(QStringLiteral("Android producer is unavailable")); return; }
    if (!d->channel.write(encodePacket({PacketKind::Refresh,d->key,{}})))
        d->revoke(QStringLiteral("Android producer refresh could not be dispatched"));
}
LaunchSubmission DescriptorRuntimePort::requestLaunch(const LaunchRequest &request)
{
    if (!producerLive()) return {false,QStringLiteral("Android producer is unavailable")};
    if (const auto prior=d->ledger.constFind(request.operationId);prior!=d->ledger.cend()) {
        const auto remembered=*prior;
        if (!sameRequest(remembered.request,request)) return {false,QStringLiteral("Android operation id was reused with different data")};
        if (remembered.result) Q_EMIT launchFinished(*remembered.result);
        return remembered.submission;
    }
    if (d->ledger.size()>=256) return {false,QStringLiteral("Android operation history is full; reconnect the producer")};
    const auto packet=encodePacket({PacketKind::Launch,d->key,request});
    if (packet.isEmpty()) return {false,QStringLiteral("Android launch request is invalid")};
    LaunchSubmission submission{false,QStringLiteral("Android launch is not currently admitted")};
    if (producerLive() && admittedLaunch(d->current,request)) {
        int pending=0; bool appBusy=false;
        for (const auto &entry:d->ledger) if (entry.submission.accepted && !entry.terminal) {
            ++pending; appBusy |= entry.request.application==request.application;
        }
        if (!appBusy && pending<64) {
            submission.accepted=d->channel.write(packet);
            submission.message=submission.accepted?QString():QStringLiteral("Android launch dispatch could not be confirmed");
        }
    }
    d->ledger.insert(request.operationId,{request,submission,!submission.accepted});
    return submission;
}
} // namespace QindaQt::Services::AndroidRuntime
