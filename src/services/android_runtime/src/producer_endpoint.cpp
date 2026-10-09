// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/services/android_runtime/producer_endpoint.h"
#include "packet_channel_p.h"
#include <QHash>
#include <QPointer>
#include <QSocketNotifier>
namespace QindaQt::Services::AndroidRuntime {
namespace {
bool sameRequest(const LaunchRequest &a,const LaunchRequest &b) {
    return a.operationId==b.operationId && a.attachmentId==b.attachmentId
        && a.snapshotRevision==b.snapshotRevision && a.application==b.application;
}
bool admitted(const RuntimeSnapshot &snapshot,const LaunchRequest &request) {
    if (snapshot.state!=RuntimeState::Ready || request.attachmentId!=snapshot.attachmentId
        || request.snapshotRevision!=snapshot.revision) return false;
    for (const auto &status:snapshot.applications)
        if (status.binding==request.application)
            return status.launchAvailable && (status.state==ApplicationState::Ready || status.state==ApplicationState::Running);
    return false;
}
}
class ProducerEndpoint::Private {
public:
    struct Operation { LaunchRequest request; std::optional<LaunchResult> result; };
    ProducerEndpoint &owner;
    PacketChannel channel;
    QByteArray key;
    RuntimeSnapshot current;
    QHash<QString,Operation> ledger;
    std::unique_ptr<QSocketNotifier> input,lifetime;
    Private(ProducerEndpoint &endpoint,int socket,int pidfd,int executable,int pid,unsigned uid,QByteArray secret)
        : owner(endpoint),channel(socket,pidfd,executable,pid,uid),key(std::move(secret)) {}
    void close() {
        if (input) input->setEnabled(false);
        if (lifetime) lifetime->setEnabled(false);
        channel.close(); current={}; Q_EMIT owner.disconnected();
    }
    void drain() {
        QPointer<ProducerEndpoint> guard(&owner);
        for (int i=0;i<64;++i) {
            const auto received=channel.read();
            if (received.state==ReadPacket::Empty) return;
            if (received.state!=ReadPacket::Received) { close(); return; }
            const auto packet=decodePacket(received.bytes);
            if (!packet || packet->channelKey!=key) { close(); return; }
            if (packet->kind==PacketKind::Refresh) {
                Q_EMIT owner.refreshRequested();
            } else if (packet->kind==PacketKind::Launch) {
                const auto request=std::get<LaunchRequest>(packet->value);
                if (const auto prior=ledger.constFind(request.operationId);prior!=ledger.cend()) {
                    if (!sameRequest(prior->request,request)) { close(); return; }
                    const auto result=prior->result;
                    if (result) channel.write(encodePacket({PacketKind::Result,key,*result}));
                    continue; // retained ID never invokes the operation again
                }
                int pending=0; bool appBusy=false;
                for (const auto &operation:ledger)
                    if (!operation.result || operation.result->phase==LaunchPhase::Accepted) {
                        ++pending; appBusy |= operation.request.application==request.application;
                    }
                if (ledger.size()>=256 || pending>=64 || appBusy || !admitted(current,request)) {
                    const LaunchResult refused{request.operationId,request.attachmentId,request.application,
                        LaunchPhase::Failed,QStringLiteral("Android launch is not admitted by the current producer")};
                    channel.write(encodePacket({PacketKind::Result,key,refused}));
                    if (ledger.size()<256) ledger.insert(request.operationId,{request,refused});
                    continue;
                }
                ledger.insert(request.operationId,{request,std::nullopt});
                Q_EMIT owner.launchRequested(request);
            } else { close(); return; }
            if (!guard) return;
            if (!channel.live()) { close(); return; }
        }
    }
};
ProducerEndpoint::ProducerEndpoint(int socket,int pidfd,int executable,int pid,unsigned uid,QByteArray key,QObject *parent)
    : QObject(parent),d(std::make_unique<Private>(*this,socket,pidfd,executable,pid,uid,std::move(key)))
{
    if (d->key.size()!=32 || !d->channel.live()) { d->channel.close(); return; }
    d->input=std::make_unique<QSocketNotifier>(d->channel.socketFd(),QSocketNotifier::Read,this);
    d->lifetime=std::make_unique<QSocketNotifier>(d->channel.lifetimeFd(),QSocketNotifier::Read,this);
    connect(d->input.get(),&QSocketNotifier::activated,this,[this]{ d->drain(); });
    connect(d->lifetime.get(),&QSocketNotifier::activated,this,[this]{ d->close(); });
}
ProducerEndpoint::~ProducerEndpoint()=default;
bool ProducerEndpoint::live() const { return d->channel.live(); }
bool ProducerEndpoint::publishSnapshot(const RuntimeSnapshot &snapshot)
{
    if (!live()) return false;
    const auto packet=encodePacket({PacketKind::Snapshot,d->key,snapshot});
    if (packet.isEmpty() || !d->channel.write(packet)) return false;
    d->current=snapshot;
    return true;
}
bool ProducerEndpoint::publishResult(const LaunchResult &result)
{
    if (!live()) return false;
    const auto operation=d->ledger.find(result.operationId);
    if (operation==d->ledger.end() || operation->request.attachmentId!=result.attachmentId
        || operation->request.application!=result.application) return false;
    if (operation->result && operation->result->phase!=LaunchPhase::Accepted) return false;
    const auto packet=encodePacket({PacketKind::Result,d->key,result});
    if (packet.isEmpty() || !d->channel.write(packet)) return false;
    operation->result=result;
    return true;
}
} // namespace QindaQt::Services::AndroidRuntime
